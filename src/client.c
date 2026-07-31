#include "minitcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SERVER_PORT 8080
#define TOTAL_PACKETS 10

int main() {
    enable_windows_ansi_terminal();
    srand((unsigned int)time(NULL));

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    SOCKET sockfd;
    struct sockaddr_in server_addr;
    int addr_len = sizeof(server_addr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint32_t base = 0;
    uint32_t next_seq_num = 0;
    Packet window_buffer[TOTAL_PACKETS];
    PacketState pkt_states[TOTAL_PACKETS];
    TransportStats stats = {0};
    char last_event[128] = "Initializing transmission window...";

    for (int i = 0; i < TOTAL_PACKETS; i++) {
        char msg[PAYLOAD_SIZE];
        snprintf(msg, PAYLOAD_SIZE, "Data_Payload_#%d", i);
        build_packet(&window_buffer[i], i, 0, FLAG_DATA, msg, (uint16_t)(strlen(msg) + 1));
        pkt_states[i] = PKT_WAIT;
    }

    render_client_dashboard(&stats, base, next_seq_num, pkt_states, TOTAL_PACKETS, last_event);

    while (base < TOTAL_PACKETS) {
        // 1. Send packets in window
        while (next_seq_num < base + WINDOW_SIZE && next_seq_num < TOTAL_PACKETS) {
            pkt_states[next_seq_num] = PKT_SENT;
            snprintf(last_event, sizeof(last_event), "Sending packet seq: %u ('%s')", 
                     next_seq_num, window_buffer[next_seq_num].payload);
            
            unreliable_sendto(sockfd, &window_buffer[next_seq_num], 
                              sizeof(MiniTCPHeader) + window_buffer[next_seq_num].header.length,
                              0, (struct sockaddr *)&server_addr, addr_len, &stats);
            
            next_seq_num++;
            render_client_dashboard(&stats, base, next_seq_num, pkt_states, TOTAL_PACKETS, last_event);
            Sleep(400); // Slight delay so you can watch the dashboard animate
        }

        // 2. Select timeout for ACK polling
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval tv;
        tv.tv_sec = TIMEOUT_SEC;
        tv.tv_usec = 0;

        int ready = select(0, &readfds, NULL, NULL, &tv);

        if (ready == SOCKET_ERROR) {
            break;
        } else if (ready == 0) {
            // 3. TIMEOUT EXPIRED: Go-Back-N Retransmission
            snprintf(last_event, sizeof(last_event), "TIMEOUT for base %u! Resending window [%u..%u]", 
                     base, base, next_seq_num - 1);
            
            for (uint32_t i = base; i < next_seq_num; i++) {
                pkt_states[i] = PKT_DROPPED; // Mark red visually
                stats.retransmissions++;
                render_client_dashboard(&stats, base, next_seq_num, pkt_states, TOTAL_PACKETS, last_event);
                Sleep(200);

                unreliable_sendto(sockfd, &window_buffer[i], 
                                  sizeof(MiniTCPHeader) + window_buffer[i].header.length,
                                  0, (struct sockaddr *)&server_addr, addr_len, &stats);
                pkt_states[i] = PKT_SENT;
            }
        } else {
            // 4. ACK received: Advance the sliding window
            Packet ack_pkt;
            recvfrom(sockfd, (char *)&ack_pkt, sizeof(MiniTCPHeader), 0, NULL, NULL);

            if (ack_pkt.header.flags & FLAG_ACK) {
                snprintf(last_event, sizeof(last_event), "Received cumulative ACK for seq: %u", ack_pkt.header.ack_num);
                if (ack_pkt.header.ack_num >= base) {
                    for (uint32_t k = base; k <= ack_pkt.header.ack_num && k < TOTAL_PACKETS; k++) {
                        pkt_states[k] = PKT_ACKED; // Mark green
                        stats.total_acked++;
                    }
                    base = ack_pkt.header.ack_num + 1;
                }
                render_client_dashboard(&stats, base, next_seq_num, pkt_states, TOTAL_PACKETS, last_event);
            }
        }
    }

    snprintf(last_event, sizeof(last_event), "SUCCESS: All %d packets acknowledged!", TOTAL_PACKETS);
    render_client_dashboard(&stats, base, next_seq_num, pkt_states, TOTAL_PACKETS, last_event);

    closesocket(sockfd);
    WSACleanup();
    return 0;
}