#include "minitcp.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int unreliable_sendto(SOCKET sockfd, const void *buf, size_t len, int flags,
const struct sockaddr *dest_addr, int addrlen, TransportStats *stats) {

const Packet *pkt = (const Packet *)buf;

// ONLY simulate drops on pure DATA packets.
// Real TCP has complex separate timers for SYN/FIN drops. We want to test data window recovery.
if ((pkt->header.flags & FLAG_DATA) && !(pkt->header.flags & FLAG_SYN) && !(pkt->header.flags & FLAG_FIN)) {
    
    // Re-seed slightly based on current packet sequence and time to prevent identical consecutive rolls
    srand((unsigned int)time(NULL) + pkt->header.seq_num);
    int roll = rand() % 100;
    
    if (roll < LOSS_RATE_PERCENT) {
        if (stats) stats->total_dropped++;
        return (int)len; // Simulated drop - fake success to caller
    }
}

if (stats) {
    stats->total_sent++;
    stats->total_bytes += pkt->header.length;
}

return sendto(sockfd, (const char *)buf, (int)len, flags, dest_addr, addrlen);


}