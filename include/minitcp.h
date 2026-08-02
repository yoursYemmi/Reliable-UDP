#ifndef MINITCP_H
#define MINITCP_H

#include <stdint.h>
#include <stddef.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "dashboard.h"

#define PAYLOAD_SIZE 512
#define WINDOW_SIZE 8
#define LOSS_RATE_PERCENT 20 

// RTT Boundaries (in milliseconds)
#define MIN_RTO_MS 200
#define MAX_RTO_MS 3000
#define INITIAL_RTO_MS 1000

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
    uint64_t send_timestamp_ms; // Added for RTT calculation
} MiniTCPHeader;

typedef struct {
    MiniTCPHeader header;
    char payload[PAYLOAD_SIZE];
} Packet;

// File transfer SYN metadata payload
typedef struct {
    char filename[128];
    uint32_t file_size;
} FileMetadata;
#pragma pack(pop)

// Dynamic RTT Tracker (RFC 6298)
typedef struct {
    float srtt;    // Smoothed Round-Trip Time
    float rttvar;  // RTT Variation
    int rto_ms;    // Active Retransmission Timeout in ms
} RTTTracker;

// Function Prototypes
uint16_t calculate_checksum(const void *data, size_t len);
void build_packet(Packet *pkt, uint32_t seq, uint32_t ack, uint16_t flags, const char *data, uint16_t len);
int unreliable_sendto(SOCKET sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, int addrlen, TransportStats *stats);

// RTT Helpers
uint64_t get_current_time_ms(void);
void init_rtt_tracker(RTTTracker *tracker);
void update_rto(RTTTracker *tracker, uint64_t sample_rtt_ms);

#endif // MINITCP_H