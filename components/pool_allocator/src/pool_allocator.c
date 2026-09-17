#include "pool_allocator.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ALIGN4(size) (((size) + 3) & ~3)

typedef struct BlockHeader {
    size_t size;
    int free;
    struct BlockHeader* next;
} BlockHeader;

#define BLOCK_HEADER_SIZE sizeof(BlockHeader)

// Blocks are kept in address order and tile the whole pool.
static uint8_t* memory_pool = NULL;
static size_t memory_pool_size = 0;
static BlockHeader* free_list = NULL;
// Lower bound for the first free block: every block before it is in use, so
// allocations start scanning here. NULL means there is no free block at all.
static BlockHeader* first_free = NULL;

void pool_create(void* mem, size_t size) {
    memory_pool = (uint8_t*)mem;
    memory_pool_size = size;

    free_list = (BlockHeader*)memory_pool;
    free_list->size = size - BLOCK_HEADER_SIZE;
    free_list->free = 1;
    free_list->next = NULL;
    first_free = free_list;
}

int pool_contains(const void* ptr) {
    return (ptr >= (const void*)memory_pool &&
            ptr < (const void*)(memory_pool + memory_pool_size));
}

void* pool_alloc(size_t size) {
    size = ALIGN4(size);
    BlockHeader* curr = first_free;
    BlockHeader* first_seen = NULL;  // first free block found by this scan

    while (curr) {
        if (curr->free) {
            // Free blocks are coalesced lazily: merge the run of free blocks that follows.
            while (curr->next && curr->next->free) {
                curr->size += BLOCK_HEADER_SIZE + curr->next->size;
                curr->next = curr->next->next;
            }

            if (curr->size >= size) {
                if (curr->size >= size + BLOCK_HEADER_SIZE + 4) {
                    // Split block
                    BlockHeader* new_block = (BlockHeader*)((uint8_t*)curr + BLOCK_HEADER_SIZE + size);
                    new_block->size = curr->size - size - BLOCK_HEADER_SIZE;
                    new_block->free = 1;
                    new_block->next = curr->next;

                    curr->size = size;
                    curr->next = new_block;
                }

                curr->free = 0;
                first_free = first_seen ? first_seen : curr->next;
                return (void*)((uint8_t*)curr + BLOCK_HEADER_SIZE);
            }

            if (!first_seen) {
                first_seen = curr;
            }
        }

        curr = curr->next;
    }

    first_free = first_seen;
    return NULL; // Out of memory
}

void pool_free(void* ptr) {
    if (!ptr) return;

    if (!pool_contains(ptr)) {
        // Pointer is not in the memory pool
        return;
    }

    BlockHeader* block = (BlockHeader*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    block->free = 1;

    // Merge with the following block now; runs of free blocks further along are
    // merged by pool_alloc() as it scans past them.
    if (block->next && block->next->free) {
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
    }

    if (!first_free || block < first_free) {
        first_free = block;
    }
}

size_t pool_block_size(const void* ptr) {
    if (!ptr || !pool_contains(ptr)) return 0;
    const BlockHeader* block = (const BlockHeader*)((const uint8_t*)ptr - BLOCK_HEADER_SIZE);
    return block->size;
}

void pool_walk(void (*callback)(void* ptr, size_t size, int used, void* user), void* user) {
    for (BlockHeader* curr = free_list; curr; curr = curr->next) {
        callback((uint8_t*)curr + BLOCK_HEADER_SIZE, curr->size, !curr->free, user);
    }
}

void pool_destroy() {
    memory_pool = NULL;
    memory_pool_size = 0;
    free_list = NULL;
    first_free = NULL;
}
