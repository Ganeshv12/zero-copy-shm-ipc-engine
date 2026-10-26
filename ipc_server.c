#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include "master_ipc_bus.h"

static int shm_fd = -1;
static master_ipc_bus_t *bus = NULL;
static volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(void) {
    signal(SIGINT, handle_sigint);

    printf("[Server] Creating shared memory object: %s\n", SHM_NAME);

    /* 1. Create or open shared memory object */
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }

    /* 2. Set the size of the shared memory region */
    if (ftruncate(shm_fd, sizeof(master_ipc_bus_t)) == -1) {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }

    /* 3. Map shared memory into virtual address space */
    bus = mmap(NULL, sizeof(master_ipc_bus_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (bus == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    /* 4. Initialize Process-Shared Synchronization Primitives */
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&bus->bus_mutex, &mattr);
    pthread_mutexattr_destroy(&mattr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&bus->bus_cond, &cattr);
    pthread_condattr_destroy(&cattr);

    /* 5. Initialize Custom Memory Pool */
    pthread_mutex_lock(&bus->bus_mutex);
    bus->pool.total_blocks = NUM_BLOCKS;
    bus->pool.free_count = NUM_BLOCKS;
    for (uint32_t i = 0; i < NUM_BLOCKS; i++) {
        bus->pool.blocks[i].is_free = true;
        bus->pool.blocks[i].block_id = i;
        memset(bus->pool.blocks[i].data, 0, BLOCK_SIZE);
    }

    /* 6. Initialize Ring Buffer */
    bus->ring.head = 0;
    bus->ring.tail = 0;
    bus->ring.count = 0;
    bus->total_messages_transferred = 0;
    pthread_mutex_unlock(&bus->bus_mutex);

    printf("[Server] IPC Bus initialized successfully. Waiting for zero-copy client messages...\n");

    /* 7. Consumer Main Loop */
    while (keep_running) {
        pthread_mutex_lock(&bus->bus_mutex);

        while (bus->ring.count == 0 && keep_running) {
            pthread_cond_wait(&bus->bus_cond, &bus->bus_mutex);
        }

        if (!keep_running) {
            pthread_mutex_unlock(&bus->bus_mutex);
            break;
        }

        /* Pop block index from ring buffer (Zero-Copy Read) */
        uint32_t block_idx = bus->ring.block_indices[bus->ring.head];
        bus->ring.head = (bus->ring.head + 1) % RING_CAPACITY;
        bus->ring.count--;

        memory_block_t *block = &bus->pool.blocks[block_idx];
        
        // Copy payload out for logging (or process directly in shared memory)
        char msg_copy[BLOCK_SIZE];
        memcpy(msg_copy, block->data, BLOCK_SIZE);

        /* Mark block free in custom memory pool */
        block->is_free = true;
        bus->pool.free_count++;
        bus->total_messages_transferred++;

        pthread_mutex_unlock(&bus->bus_mutex);

        printf("[Server Rx] Block ID %u Processed | Payload: %s\n", block_idx, msg_copy);
    }

    /* 8. Cleanup and Unlink */
    printf("\n[Server] Shutting down and cleaning up shared memory...\n");
    
    pthread_mutex_destroy(&bus->bus_mutex);
    pthread_cond_destroy(&bus->bus_cond);
    munmap(bus, sizeof(master_ipc_bus_t));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    printf("[Server] Cleanup complete.\n");
    return EXIT_SUCCESS;
}
