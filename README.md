# Enterprise-Grade Zero-Copy Shared Memory IPC Engine in C

A high-performance, microkernel-style inter-process communication (IPC) engine built from scratch in C. It eliminates OS kernel context-switch overhead for data payloads by uniting **POSIX Shared Memory (`mmap`)**, an **Embedded Static Memory Pool Allocator**, and **Process-Shared Synchronization (`PTHREAD_PROCESS_SHARED`)** into a single unified architecture.

## 1. System Architecture

    [ Independent Client Process ] 
          | (1. shm_open / mmap)
          v
    +-------------------------------------------------------+
    |               SHARED MEMORY REGION                    |
    |                                                       |
    |  - Process-Shared Primitives (Mutex & Cond Variable)  |
    |  - Custom Memory Pool (Fixed-size block allocation)   |
    |  - Zero-Copy Ring Buffer (Index-based referencing)    |
    +-------------------------------------------------------+
          ^
          | (2. Direct Block Read & Zero-Copy Processing)
    [ Independent Server Daemon ]

## 2. Core Technical Features
- **Zero-Copy Data Transfer:** Producers and consumers map the exact same physical RAM segment, writing payloads directly into shared blocks without streaming through kernel pipes or sockets.
- **Process-Shared Synchronization:** Leverages `pthread_mutexattr_setpshared()` and `pthread_condattr_setpshared()` so locks and condition variables operate seamlessly across *independent processes*, not just threads.
- **Embedded Custom Allocator:** Manages a pre-allocated fixed-block memory pool directly inside the shared memory segment, avoiding unsafe runtime `malloc()` calls across address spaces.
- **Safe Index Referencing:** Uses safe relative indices rather than absolute memory pointers to prevent pointer corruption and segmentation faults across virtual address spaces.

## 3. Build and Run
1. Compile the binaries:
   `make`
2. Start the server daemon (Terminal 1):
   `./ipc_server`
3. Send zero-copy messages from independent client processes (Terminal 2):
   `./ipc_client "Telemetry Node 01: Core temperature nominal at 45C"`
   `./ipc_client "Emergency Alert: Pressure drop detected in valve B!"`
