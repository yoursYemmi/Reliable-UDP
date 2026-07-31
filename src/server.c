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
    char last_event[128] = "Server initialized and waiting for client packets...";

    render_server_dashboard(&stats, expected_seq, last_event);

    while (1) {
        int bytes = recvfrom(sockfd, (char *)&recv_pkt, sizeof(Packet), 0,
                             (struct sockaddr *)&client_addr, &addr_len);
        if (bytes <= 0) continue;

        stats.total_sent++; // Track total incoming packets

        // Verify Checksum
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
            snprintf(last_event, sizeof(last_event), "Accepted IN-ORDER seq %u ('%s')", 
                     recv_pkt.header.seq_num, recv_pkt.payload);
            
            build_packet(&ack_pkt, 0, expected_seq, FLAG_ACK, NULL, 0);
            unreliable_sendto(sockfd, &ack_pkt, sizeof(MiniTCPHeader), 0,
                              (struct sockaddr *)&client_addr, addr_len, NULL);
            
            expected_seq++;
        } else {
            stats.total_dropped++; // Track out-of-order discards
            snprintf(last_event, sizeof(last_event), "OUT-OF-ORDER (got %u, expected %u). Re-ACKing %d",
                     recv_pkt.header.seq_num, expected_seq, (int)expected_seq - 1);
            
            uint32_t ack_to_send = (expected_seq > 0) ? (expected_seq - 1) : 0;
            build_packet(&ack_pkt, 0, ack_to_send, FLAG_ACK, NULL, 0);
            unreliable_sendto(sockfd, &ack_pkt, sizeof(MiniTCPHeader), 0,
                              (struct sockaddr *)&client_addr, addr_len, NULL);
        }

        render_server_dashboard(&stats, expected_seq, last_event);
    }

    closesocket(sockfd);
    WSACleanup();
    return 0;
}