#include "minitcp.h"
#include <stdlib.h>
#include <stdio.h>

int unreliable_sendto(SOCKET sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, int addrlen, TransportStats *stats) {
    int roll = rand() % 100;
    const Packet *pkt = (const Packet *)buf;
    
    if (roll < LOSS_RATE_PERCENT) {
        if (stats) stats->total_dropped++;
        return (int)len; // Simulated drop
    }

    if (stats) {
        stats->total_sent++;
        stats->total_bytes += pkt->header.length;
    }

    return sendto(sockfd, (const char *)buf, (int)len, flags, dest_addr, addrlen);
}