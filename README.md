# Reliable-UDP
# ⚡ Mini-TCP: Reliable Transport Protocol & TUI Dashboard over UDP

A user-space reliable transport protocol implemented in **C (WinSock2)** that guarantees ordered, error-checked binary file delivery over unreliable UDP datagrams.

Features a real-time **ANSI Virtual Terminal TUI Dashboard** and a built-in **stochastic loss emulator** to visualize sliding window flow control and retransmissions under simulated network degradation.

---

## 🏛️ Architecture & Features

- **Sliding Window Flow Control (Go-Back-N):** Supports pipelined transmission with cumulative acknowledgments and sequential window sliding.
- **Adaptive RTT & Exponential Backoff (RFC 6298):** Implements Jacobson's Exponentially Weighted Moving Average (EWMA) algorithm to calculate `SRTT` and `RTTVAR`, dynamically tuning retransmission timeouts (`RTO`) between `200ms` and `3000ms`.
- **Custom Packet Framing & Verification:** 16-byte aligned binary wire header featuring one's-complement Internet Checksums (RFC 1071), sequence numbering, and SYN/DATA/FIN flag handshaking.
- **Binary Asset Streaming:** Streams files of arbitrary size via dynamic memory chunking and direct-to-disk stream buffers without RAM exhaustion.
- **Observability TUI:** Zero-flicker Windows Console ANSI renderer displaying live Goodput %, packet drop telemetry, and visual sliding-window state tiles (`[ACK]`, `[SENT]`, `[DROP]`, `[WAIT]`).

---

## 🚀 Build & Run (Windows PowerShell / MinGW-w64)

### 1. Compile the Project
```powershell
mingw32-make clean
mingw32-make
