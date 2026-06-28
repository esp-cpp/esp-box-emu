#include "shared_memory.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

#define MAX_SHARED_ALLOCATIONS 256

typedef struct {
    void *raw_ptr;
    void *aligned_ptr;
    size_t size;
    shared_mem_storage_t storage;
} shared_mem_allocation_t;

static shared_mem_allocation_t allocations_[MAX_SHARED_ALLOCATIONS];
static size_t allocation_count_ = 0;
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

static int get_storage_caps(shared_mem_storage_t storage) {
    switch (storage) {
        case SHARED_MEM_INTERNAL:
            return MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
        case SHARED_MEM_PSRAM:
        default:
            return MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    }
}

static void *align_pointer(const void *ptr, size_t alignment) {
    uintptr_t raw_address = (uintptr_t)ptr;
    uintptr_t aligned_address = (raw_address + (alignment - 1)) & ~((uintptr_t)alignment - 1);
    return (void *)aligned_address;
}

void* shared_mem_get_instance(void) {
    if (allocation_count_ == 0) {
        return NULL;
    }
    return allocations_[0].aligned_ptr;
}

void* shared_malloc(size_t size) {
    shared_mem_request_t request = {
        .size = size,
        .region = SHARED_MEM_DEFAULT,
        .storage = SHARED_MEM_PSRAM,
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

    if (allocation_count_ >= MAX_SHARED_ALLOCATIONS) {
        return NULL;
    }

    // Calculate alignment offset
    size_t alignment = get_alignment_offset(request->region);
    size_t allocation_size = request->size + alignment - 1;
    void *raw_ptr = heap_caps_malloc(allocation_size, get_storage_caps(request->storage));
    if (raw_ptr == NULL) {
        return NULL;
    }

    void *aligned_ptr = align_pointer(raw_ptr, alignment);
    memset(aligned_ptr, 0, request->size);

    allocations_[allocation_count_++] = (shared_mem_allocation_t){
        .raw_ptr = raw_ptr,
        .aligned_ptr = aligned_ptr,
        .size = request->size,
        .storage = request->storage,
    };
    current_offset_ += request->size;

    return aligned_ptr;
}

void shared_mem_clear(void) {
    printf("Num bytes allocated: %d\n", (int)current_offset_);

    for (size_t i = 0; i < allocation_count_; i++) {
        if (allocations_[i].raw_ptr != NULL) {
            heap_caps_free(allocations_[i].raw_ptr);
        }
        allocations_[i].raw_ptr = NULL;
        allocations_[i].aligned_ptr = NULL;
        allocations_[i].size = 0;
        allocations_[i].storage = SHARED_MEM_PSRAM;
    }
    allocation_count_ = 0;
    current_offset_ = 0;
}

shared_mem_stats_t shared_mem_get_stats(void) {
    shared_mem_stats_t stats = {
        .total_allocated = current_offset_,
        .total_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
                    + heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    };
    return stats;
}
