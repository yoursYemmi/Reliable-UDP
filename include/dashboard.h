#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <stdint.h>
#include <windows.h>

// ANSI Escape Color Codes
#define ANSI_RESET   "\033[0m"
#define ANSI_GREEN   "\033[1;32m"
#define ANSI_YELLOW  "\033[1;33m"
#define ANSI_RED     "\033[1;31m"
#define ANSI_CYAN    "\033[1;36m"
#define ANSI_GRAY    "\033[1;30m"
#define ANSI_BOLD    "\033[1m"

// Packet Visual States in the Dashboard
typedef enum {
    PKT_WAIT = 0,   // Not sent yet (Gray)
    PKT_SENT = 1,   // In-flight / unacknowledged (Yellow)
    PKT_ACKED = 2,  // Acknowledged (Green)
    PKT_DROPPED = 3 // Dropped or timed out (Red)
} PacketState;

// Real-Time Network Telemetry
typedef struct {
    uint32_t total_sent;
    uint32_t total_acked;
    uint32_t total_dropped;
    uint32_t retransmissions;
    uint32_t total_bytes;
    uint32_t total_packets;
} TransportStats;

// Function Prototypes
void enable_windows_ansi_terminal(void);
void render_client_dashboard(const TransportStats *stats, uint32_t base, 
                             uint32_t next_seq, const PacketState *pkt_states, 
                             uint32_t total_pkts, const char *last_event);
void render_server_dashboard(const TransportStats *stats, uint32_t expected_seq, 
                             const char *last_event);

#endif // DASHBOARD_H