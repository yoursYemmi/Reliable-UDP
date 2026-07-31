#include "minitcp.h"
#include <string.h>

// RFC 1071 16-bit One's Complement Sum
uint16_t calculate_checksum(const void *data, size_t len) {
    const uint16_t *buf = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    if (len > 0) {
        sum += *(const uint8_t *)buf;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

void build_packet(Packet *pkt, uint32_t seq, uint32_t ack, uint16_t flags, const char *data, uint16_t len) {
    memset(pkt, 0, sizeof(Packet));
    pkt->header.seq_num = seq;
    pkt->header.ack_num = ack;
    pkt->header.flags = flags;
    pkt->header.window_size = WINDOW_SIZE;
    pkt->header.length = len;
    
    if (data && len > 0) {
        memcpy(pkt->payload, data, len);
    }

    pkt->header.checksum = 0;
    pkt->header.checksum = calculate_checksum(pkt, sizeof(MiniTCPHeader) + len);
}