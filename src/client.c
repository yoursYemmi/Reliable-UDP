#include "minitcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SERVER_PORT 8080

// Create a dummy binary file if none is provided via command line
void create_sample_file(const char *filename) {
FILE *fp = fopen(filename, "wb");
if (!fp) return;
for (int i = 0; i < 20; i++) {
fprintf(fp, "Mini-TCP File Transfer Chunk Line #%04d : Reliable UDP Streaming Test Data\n", i);
}
fclose(fp);
}

int main(int argc, char *argv[]) {
enable_windows_ansi_terminal();
srand((unsigned int)time(NULL));

const char *filepath = (argc > 1) ? argv[1] : "sample_data.txt";
if (argc <= 1) create_sample_file(filepath);

FILE *fp = fopen(filepath, "rb");
if (!fp) {
    printf("Error: Could not open file '%s'\n", filepath);
    return 1;
}

fseek(fp, 0, SEEK_END);
long file_size = ftell(fp);
fseek(fp, 0, SEEK_SET);

// Calculate total packets required: 1 SYN + Data Chunks + 1 FIN
uint32_t data_packets = (uint32_t)((file_size + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE);
uint32_t total_packets = data_packets + 2;

WSADATA wsaData;
if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
struct sockaddr_in server_addr;
int addr_len = sizeof(server_addr);
memset(&server_addr, 0, sizeof(server_addr));
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(SERVER_PORT);
server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

// Dynamically allocate window buffer for all file packets
Packet *window_buffer = (Packet *)calloc(total_packets, sizeof(Packet));
PacketState *pkt_states = (PacketState *)calloc(total_packets, sizeof(PacketState));

// 1. Build SYN Packet (seq 0)
FileMetadata meta;
memset(&meta, 0, sizeof(FileMetadata));
snprintf(meta.filename, sizeof(meta.filename), "%s", filepath);
meta.file_size = (uint32_t)file_size;
build_packet(&window_buffer[0], 0, 0, FLAG_SYN, (const char *)&meta, sizeof(FileMetadata));
pkt_states[0] = PKT_WAIT;

// 2. Build DATA Packets (seq 1 to data_packets)
for (uint32_t i = 1; i <= data_packets; i++) {
    char chunk[PAYLOAD_SIZE];
    memset(chunk, 0, PAYLOAD_SIZE);
    size_t bytes_read = fread(chunk, 1, PAYLOAD_SIZE, fp);
    build_packet(&window_buffer[i], i, 0, FLAG_DATA, chunk, (uint16_t)bytes_read);
    pkt_states[i] = PKT_WAIT;
}
fclose(fp);

// 3. Build FIN Packet (seq total_packets - 1)
build_packet(&window_buffer[total_packets - 1], total_packets - 1, 0, FLAG_FIN, "FIN", 3);
pkt_states[total_packets - 1] = PKT_WAIT;

uint32_t base = 0;
uint32_t next_seq_num = 0;
TransportStats stats = {0};
RTTTracker rtt = {0};
init_rtt_tracker(&rtt);

char last_event[128];
snprintf(last_event, sizeof(last_event), "Loaded file '%s' (%ld bytes, %u packets)", 
         filepath, file_size, total_packets);

render_client_dashboard(&stats, base, next_seq_num, pkt_states, total_packets, last_event);

while (base < total_packets) {
    // Send packets within sliding window [base, base + WINDOW_SIZE)
    while (next_seq_num < base + WINDOW_SIZE && next_seq_num < total_packets) {
        pkt_states[next_seq_num] = PKT_SENT;
        
        // --- FIX: Recalculate Checksum after modifying timestamp ---
        window_buffer[next_seq_num].header.send_timestamp_ms = get_current_time_ms();
        window_buffer[next_seq_num].header.checksum = 0; 
        window_buffer[next_seq_num].header.checksum = calculate_checksum(&window_buffer[next_seq_num], 
                                                      sizeof(MiniTCPHeader) + window_buffer[next_seq_num].header.length);

        snprintf(last_event, sizeof(last_event), "Send seq %u (flags: 0x%x, RTO: %dms)", 
                 next_seq_num, window_buffer[next_seq_num].header.flags, rtt.rto_ms);
        
        unreliable_sendto(sockfd, &window_buffer[next_seq_num], 
                          sizeof(MiniTCPHeader) + window_buffer[next_seq_num].header.length,
                          0, (struct sockaddr *)&server_addr, addr_len, &stats);
        
        next_seq_num++;
        render_client_dashboard(&stats, base, next_seq_num, pkt_states, total_packets, last_event);
        Sleep(150); // Visual pacing for ANSI dashboard
    }

    // Setup select() with DYNAMIC RFC 6298 RTO (converted from ms to timeval)
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval tv;
    tv.tv_sec = rtt.rto_ms / 1000;
    tv.tv_usec = (rtt.rto_ms % 1000) * 1000;

    int ready = select(0, &readfds, NULL, NULL, &tv);

    if (ready == SOCKET_ERROR) {
        break;
    } else if (ready == 0) {
        // TIMEOUT EXPIRED: Go-Back-N Retransmission & Exponential Backoff
        rtt.rto_ms = (rtt.rto_ms * 2 > MAX_RTO_MS) ? MAX_RTO_MS : (rtt.rto_ms * 2);
        snprintf(last_event, sizeof(last_event), "TIMEOUT base %u! Exponential Backoff RTO: %dms", 
                 base, rtt.rto_ms);
        
        for (uint32_t i = base; i < next_seq_num; i++) {
            pkt_states[i] = PKT_DROPPED;
            stats.retransmissions++;
            render_client_dashboard(&stats, base, next_seq_num, pkt_states, total_packets, last_event);
            Sleep(80);

            // --- FIX: Recalculate Checksum for retransmissions too ---
            window_buffer[i].header.send_timestamp_ms = get_current_time_ms();
            window_buffer[i].header.checksum = 0; 
            window_buffer[i].header.checksum = calculate_checksum(&window_buffer[i], 
                                                          sizeof(MiniTCPHeader) + window_buffer[i].header.length);

            unreliable_sendto(sockfd, &window_buffer[i], 
                              sizeof(MiniTCPHeader) + window_buffer[i].header.length,
                              0, (struct sockaddr *)&server_addr, addr_len, &stats);
            pkt_states[i] = PKT_SENT;
        }
    } else {
        // ACK Received: Compute RTT & Slide Window Forward
        Packet ack_pkt;
        recvfrom(sockfd, (char *)&ack_pkt, sizeof(MiniTCPHeader), 0, NULL, NULL);

        if (ack_pkt.header.flags & FLAG_ACK) {
            uint64_t now_ms = get_current_time_ms();
            uint64_t sample_rtt = now_ms - ack_pkt.header.send_timestamp_ms;
            update_rto(&rtt, sample_rtt);

            snprintf(last_event, sizeof(last_event), "ACK seq %u received (RTT: %ums | New RTO: %dms)", 
                     ack_pkt.header.ack_num, (uint32_t)sample_rtt, rtt.rto_ms);

            if (ack_pkt.header.ack_num >= base) {
                for (uint32_t k = base; k <= ack_pkt.header.ack_num && k < total_packets; k++) {
                    pkt_states[k] = PKT_ACKED;
                    stats.total_acked++;
                }
                base = ack_pkt.header.ack_num + 1;
            }
            render_client_dashboard(&stats, base, next_seq_num, pkt_states, total_packets, last_event);
        }
    }
}

snprintf(last_event, sizeof(last_event), "SUCCESS: File '%s' fully transferred & ACKed!", filepath);
render_client_dashboard(&stats, base, next_seq_num, pkt_states, total_packets, last_event);

free(window_buffer);
free(pkt_states);
closesocket(sockfd);
WSACleanup();
return 0;


}