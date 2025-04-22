#include "shared_memory.h"
#include <string.h>
#include <stdio.h>

// Total shared memory size - can be tuned based on needs
#define TOTAL_MEMORY_SIZE (145 * 1024)  // 256KB total shared memory

// Aligned memory pool
static uint8_t memory_pool_[TOTAL_MEMORY_SIZE] __attribute__((aligned(32)));
static size_t current_offset_ = 0;

// Calculate alignment offset for a region
static size_t get_alignment_offset(shared_mem_region_t region) {
    switch (region) {
        case SHARED_MEM_VECTOR:
            return 16;
        case SHARED_MEM_CACHE_LINE:
            return 32;
        default:
            return 4;
    }
}

void* shared_mem_get_instance(void) {
    return memory_pool_;  // Return base address of memory pool
}

void* shared_malloc(size_t size) {
    shared_mem_request_t request = {
        .size = size,
        .region = SHARED_MEM_DEFAULT
    };
    return shared_mem_allocate(&request);
}

size_t shared_num_bytes_allocated(void) {
    return current_offset_;
}

void* shared_mem_allocate(const shared_mem_request_t* request) {
    if (!request) {
        return NULL;
    }

    // Calculate alignment offset
    size_t alignment = get_alignment_offset(request->region);
    size_t offset = (alignment - (current_offset_ % alignment)) % alignment;
    
    // Check if we have enough space
    if (current_offset_ + offset + request->size > TOTAL_MEMORY_SIZE) {
        return NULL;
    }

    // Calculate aligned address
    void* ptr = memory_pool_ + current_offset_ + offset;
    
    // Update current offset
    current_offset_ += offset + request->size;
    
    return ptr;
}

void shared_mem_clear(void) {
    printf("Num bytes allocated: %d\n", current_offset_);
    // Use SIMD-accelerated memset from ESP32
    // heap_caps_memset(memory_pool_, 0, TOTAL_MEMORY_SIZE);
    memset(memory_pool_, 0, TOTAL_MEMORY_SIZE);
    current_offset_ = 0;
}

shared_mem_stats_t shared_mem_get_stats(void) {
    shared_mem_stats_t stats = {
        .total_allocated = current_offset_,
        .total_free = TOTAL_MEMORY_SIZE - current_offset_
    };
    return stats;
}
