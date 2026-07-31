#include "dashboard.h"
#include <stdio.h>

// Fix for older MinGW/GCC headers where Windows 10/11 VT flags aren't defined
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

// Enable ANSI escape sequences in Windows PowerShell / cmd
void enable_windows_ansi_terminal(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

void render_client_dashboard(const TransportStats *stats, uint32_t base, 
                             uint32_t next_seq, const PacketState *pkt_states, 
                             uint32_t total_pkts, const char *last_event) {
    // Move cursor to top-left without clearing screen history (no flicker)
    printf("\033[H\033[2J");

    printf(ANSI_CYAN "========================================================================\n" ANSI_RESET);
    printf(ANSI_BOLD "        MINI-TCP (GO-BACK-N) RELIABLE TRANSPORT CLIENT DASHBOARD        \n" ANSI_RESET);
    printf(ANSI_CYAN "========================================================================\n" ANSI_RESET);

    // 1. Sliding Window Visualization
    printf("\n" ANSI_BOLD " [ SLIDING WINDOW STATE ]" ANSI_RESET "  (Base: %u | NextSeq: %u)\n ", base, next_seq);
    
    for (uint32_t i = 0; i < total_pkts; i++) {
        const char *color = ANSI_GRAY;
        const char *label = "WAIT";

        if (pkt_states[i] == PKT_ACKED) {
            color = ANSI_GREEN;
            label = "ACK ";
        } else if (pkt_states[i] == PKT_SENT) {
            color = ANSI_YELLOW;
            label = "SENT";
        } else if (pkt_states[i] == PKT_DROPPED) {
            color = ANSI_RED;
            label = "DROP";
        }

        // Highlight packets currently inside the active window [base, base + WINDOW_SIZE)
        if (i >= base && i < base + 4 && pkt_states[i] != PKT_ACKED) {
            printf("%s[#%02u:%s]" ANSI_RESET " ", color, i, label);
        } else {
            printf("%s [#%02u:%s] " ANSI_RESET, color, i, label);
        }

        if ((i + 1) % 5 == 0 && i != total_pkts - 1) printf("\n ");
    }

    // 2. Telemetry Metrics Table
    float goodput = (stats->total_sent > 0) ? 
        ((float)stats->total_acked / (float)stats->total_sent) * 100.0f : 0.0f;

    printf("\n\n" ANSI_CYAN "------------------------------------------------------------------------\n" ANSI_RESET);
    printf(ANSI_BOLD "  LIVE TELEMETRY METRICS\n" ANSI_RESET);
    printf("  +-----------------------+------------------+-------------------------+\n");
    printf("  | Total Sent:    %-6u | ACKed:    %-6u | Goodput:        %5.1f%% |\n",
           stats->total_sent, stats->total_acked, goodput);
    printf("  | Emulated Drops: %-5u | Retrans:  %-6u | Payload Bytes:  %-6u |\n",
           stats->total_dropped, stats->retransmissions, stats->total_bytes);
    printf("  +-----------------------+------------------+-------------------------+\n");

    // 3. Last Protocol Event
    printf("\n" ANSI_BOLD " [ LATEST EVENT ]: " ANSI_YELLOW "%s" ANSI_RESET "\n", last_event);
    printf(ANSI_CYAN "========================================================================\n" ANSI_RESET);
    fflush(stdout);
}

void render_server_dashboard(const TransportStats *stats, uint32_t expected_seq, 
                             const char *last_event) {
    printf("\033[H\033[2J");
    printf(ANSI_CYAN "========================================================================\n" ANSI_RESET);
    printf(ANSI_BOLD "        MINI-TCP (GO-BACK-N) RELIABLE TRANSPORT SERVER DASHBOARD        \n" ANSI_RESET);
    printf(ANSI_CYAN "========================================================================\n" ANSI_RESET);

    printf("\n" ANSI_BOLD " [ RECEIVER STATE ]" ANSI_RESET " Expected Sequence Number: " ANSI_GREEN "%u" ANSI_RESET "\n", expected_seq);

    printf("\n  +-----------------------+------------------+-------------------------+\n");
    printf("  | Packets Received: %-3u | Valid ACKs: %-4u | Out-of-Order:   %-6u |\n",
           stats->total_sent, stats->total_acked, stats->total_dropped);
    printf("  +-----------------------+------------------+-------------------------+\n");

    printf("\n" ANSI_BOLD " [ LATEST EVENT ]: " ANSI_YELLOW "%s" ANSI_RESET "\n", last_event);
    printf(ANSI_CYAN "========================================================================\n" ANSI_RESET);
    fflush(stdout);
}