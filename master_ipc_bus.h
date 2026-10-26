#ifndef MASTER_IPC_BUS_H
#define MASTER_IPC_BUS_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#define SHM_NAME "/master_ipc_bus_shm"
#define BLOCK_SIZE 128      /* Size of each memory pool block in bytes */
#define NUM_BLOCKS 64       /* Total blocks in the memory pool */
#define RING_CAPACITY 32    /* Ring buffer slot capacity */

/* 1. MEMORY POOL BLOCK STRUCTURE */
typedef struct {
    bool is_free;
    uint32_t block_id;
    uint8_t data[BLOCK_SIZE];
} memory_block_t;

/* 2. CUSTOM STATIC MEMORY POOL (Inside Shared Memory) */
typedef struct {
    uint32_t total_blocks;
    uint32_t free_count;
    memory_block_t blocks[NUM_BLOCKS];
} shared_memory_pool_t;

/* 3. ZERO-COPY RING BUFFER (Stores relative offsets or block indices) */
typedef struct {
    uint32_t block_indices[RING_CAPACITY]; /* Indices into the shared memory pool blocks */
    int head;
    int tail;
    int count;
} shared_ring_buffer_t;

/* 4. MASTER UNIFIED SHARED MEMORY CONTROL BLOCK */
typedef struct {
    /* Process-Shared Synchronization Primitives */
    pthread_mutexattr_t mutex_attr;
    pthread_mutex_t bus_mutex;
    pthread_condattr_t cond_attr;
    pthread_cond_t bus_cond;

    /* Subsystems inside Shared RAM */
    shared_memory_pool_t pool;
    shared_ring_buffer_t ring;

    /* Global Telemetry Metrics */
    uint64_t total_messages_transferred;
} master_ipc_bus_t;

#endif
