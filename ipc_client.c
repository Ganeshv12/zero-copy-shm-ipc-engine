#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "master_ipc_bus.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s \"<message_payload>\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *payload = argv[1];

    /* 1. Open the existing shared memory object created by the server */
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed. Is the ipc_server running?");
        exit(EXIT_FAILURE);
    }

    /* 2. Map shared memory into virtual address space */
    master_ipc_bus_t *bus = mmap(NULL, sizeof(master_ipc_bus_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (bus == MAP_FAILED) {
        perror("mmap failed");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    /* 3. Acquire process-shared mutex */
    pthread_mutex_lock(&bus->bus_mutex);

    /* Check if ring buffer or memory pool is full */
    if (bus->ring.count >= RING_CAPACITY || bus->pool.free_count == 0) {
        fprintf(stderr, "[Client Error] IPC bus is saturated (Ring full or Pool exhausted)!\n");
        pthread_mutex_unlock(&bus->bus_mutex);
        munmap(bus, sizeof(master_ipc_bus_t));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    /* 4. Allocate a block from the custom shared memory pool */
    int allocated_index = -1;
    for (uint32_t i = 0; i < NUM_BLOCKS; i++) {
        if (bus->pool.blocks[i].is_free) {
            bus->pool.blocks[i].is_free = false;
            allocated_index = (int)i;
            bus->pool.free_count--;
            break;
        }
    }

    if (allocated_index == -1) {
        fprintf(stderr, "[Client Error] No free blocks found in memory pool.\n");
        pthread_mutex_unlock(&bus->bus_mutex);
        munmap(bus, sizeof(master_ipc_bus_t));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    /* 5. Write payload directly into the shared block (Zero-Copy Write) */
    memory_block_t *block = &bus->pool.blocks[allocated_index];
    memset(block->data, 0, BLOCK_SIZE);
    snprintf((char *)block->data, BLOCK_SIZE, "%s", payload);

    /* 6. Push block index into ring buffer */
    bus->ring.block_indices[bus->ring.tail] = (uint32_t)allocated_index;
    bus->ring.tail = (bus->ring.tail + 1) % RING_CAPACITY;
    bus->ring.count++;

    printf("[Client Tx] Allocated Shared Pool Block ID %d | Payload Written: \"%s\"\n", 
           allocated_index, payload);

    /* 7. Unlock mutex and signal server condition variable across processes */
    pthread_mutex_unlock(&bus->bus_mutex);
    pthread_cond_signal(&bus->bus_cond);

    /* Cleanup local resource mapping */
    munmap(bus, sizeof(master_ipc_bus_t));
    close(shm_fd);

    return EXIT_SUCCESS;
}
