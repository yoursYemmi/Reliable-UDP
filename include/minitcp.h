#ifndef MINITCP_H
#define MINITCP_H

#include <stdint.h>
#include <stddef.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "dashboard.h"

#define PAYLOAD_SIZE 512
#define WINDOW_SIZE 4
#define TIMEOUT_SEC 1       
#define LOSS_RATE_PERCENT 25 

#define FLAG_DATA 0x01
#define FLAG_ACK  0x02
#define FLAG_SYN  0x04
#define FLAG_FIN  0x08

#pragma pack(push, 1)
typedef struct {
    uint32_t seq_num;     
    uint32_t ack_num;     
    uint16_t flags;       
    uint16_t window_size; 
    uint16_t checksum;    
    uint16_t length;      
} MiniTCPHeader;

typedef struct {
    MiniTCPHeader header;
    char payload[PAYLOAD_SIZE];
} Packet;
#pragma pack(pop)

uint16_t calculate_checksum(const void *data, size_t len);
void build_packet(Packet *pkt, uint32_t seq, uint32_t ack, uint16_t flags, const char *data, uint16_t len);
int unreliable_sendto(SOCKET sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, int addrlen, TransportStats *stats);

#endif // MINITCP_H