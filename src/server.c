#include "minitcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080

int main() {
    enable_windows_ansi_terminal();
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    SOCKET sockfd;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    uint32_t expected_seq = 0;
    Packet recv_pkt, ack_pkt;
    TransportStats stats = {0};
    char last_event[128] = "Server ready. Waiting for SYN file transfer...";

    FILE *out_fp = NULL;
    char out_filename[150] = {0};

    render_server_dashboard(&stats, expected_seq, last_event);

    while (1) {
        int bytes = recvfrom(sockfd, (char *)&recv_pkt, sizeof(Packet), 0,
                             (struct sockaddr *)&client_addr, &addr_len);
        if (bytes <= 0) continue;

        stats.total_sent++;
        uint16_t recv_checksum = recv_pkt.header.checksum;
        recv_pkt.header.checksum = 0;
        uint16_t calc_checksum = calculate_checksum(&recv_pkt, sizeof(MiniTCPHeader) + recv_pkt.header.length);

        if (recv_checksum != calc_checksum) {
            snprintf(last_event, sizeof(last_event), "Corrupt packet discarded (seq: %u)", recv_pkt.header.seq_num);
            render_server_dashboard(&stats, expected_seq, last_event);
            continue;
        }

        if (recv_pkt.header.seq_num == expected_seq) {
            stats.total_acked++;

            // 1. Handle SYN Handshake (Open Output File)
            if (recv_pkt.header.flags & FLAG_SYN) {
                FileMetadata *meta = (FileMetadata *)recv_pkt.payload;
                snprintf(out_filename, sizeof(out_filename), "received_%s", meta->filename);
                out_fp = fopen(out_filename, "wb");
                snprintf(last_event, sizeof(last_event), "SYN: Receiving file '%s' (%u bytes)", 
                         out_filename, meta->file_size);
            } 
            // 2. Handle DATA Packets (Write Chunk to Disk)
            else if (recv_pkt.header.flags & FLAG_DATA && out_fp) {
                fwrite(recv_pkt.payload, 1, recv_pkt.header.length, out_fp);
                snprintf(last_event, sizeof(last_event), "Written chunk seq %u (%u bytes)", 
                         recv_pkt.header.seq_num, recv_pkt.header.length);
            } 
            // 3. Handle FIN Handshake (Close File)
            else if (recv_pkt.header.flags & FLAG_FIN) {
                if (out_fp) fclose(out_fp);
                out_fp = NULL;
                snprintf(last_event, sizeof(last_event), "FIN: File '%s' successfully saved to disk!", out_filename);
            }

            // Send Cumulative ACK back to sender
            build_packet(&ack_pkt, 0, expected_seq, FLAG_ACK, NULL, 0);
            ack_pkt.header.send_timestamp_ms = recv_pkt.header.send_timestamp_ms; // Echo timestamp for RTT
            unreliable_sendto(sockfd, &ack_pkt, sizeof(MiniTCPHeader), 0,
                              (struct sockaddr *)&client_addr, addr_len, NULL);
            
            expected_seq++;
        } else {
            stats.total_dropped++;
            snprintf(last_event, sizeof(last_event), "OUT-OF-ORDER (got %u, expected %u). Re-ACKing %d",
                     recv_pkt.header.seq_num, expected_seq, (int)expected_seq - 1);
            
            uint32_t ack_to_send = (expected_seq > 0) ? (expected_seq - 1) : 0;
            build_packet(&ack_pkt, 0, ack_to_send, FLAG_ACK, NULL, 0);
            unreliable_sendto(sockfd, &ack_pkt, sizeof(MiniTCPHeader), 0,
                              (struct sockaddr *)&client_addr, addr_len, NULL);
        }

        render_server_dashboard(&stats, expected_seq, last_event);
    }

    if (out_fp) fclose(out_fp);
    closesocket(sockfd);
    WSACleanup();
    return 0;
}