#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memory regions for different alignment requirements
typedef enum {
    SHARED_MEM_DEFAULT,    // 4-byte aligned
    SHARED_MEM_VECTOR,     // 16-byte aligned for SIMD operations
    SHARED_MEM_CACHE_LINE  // 32-byte aligned for cache line optimization
} shared_mem_region_t;

// Memory allocation request
typedef struct {
    size_t size;
    shared_mem_region_t region;
} shared_mem_request_t;

// Memory usage statistics
typedef struct {
    size_t total_allocated;
    size_t total_free;
} shared_mem_stats_t;

// Get singleton instance
void* shared_mem_get_instance(void);

// Allocate memory with specific alignment requirements
void* shared_mem_allocate(const shared_mem_request_t* request);

void* shared_malloc(size_t size);

size_t shared_num_bytes_allocated(void);

// Clear all memory (using SIMD-accelerated memset)
void shared_mem_clear(void);

// Get current memory usage statistics
shared_mem_stats_t shared_mem_get_stats(void);

#ifdef __cplusplus
}
#endif
