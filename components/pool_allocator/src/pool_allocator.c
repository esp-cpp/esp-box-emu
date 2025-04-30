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

static uint8_t* memory_pool = NULL;
static size_t memory_pool_size = 0;
static BlockHeader* free_list = NULL;

void pool_create(void* mem, size_t size) {
    memory_pool = (uint8_t*)mem;
    memory_pool_size = size;

    free_list = (BlockHeader*)memory_pool;
    free_list->size = size - BLOCK_HEADER_SIZE;
    free_list->free = 1;
    free_list->next = NULL;
}

void* pool_alloc(size_t size) {
    size = ALIGN4(size);
    BlockHeader* curr = free_list;

    while (curr) {
        if (curr->free && curr->size >= size) {
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
            return (void*)((uint8_t*)curr + BLOCK_HEADER_SIZE);
        }

        curr = curr->next;
    }

    return NULL; // Out of memory
}

void pool_free(void* ptr) {
    if (!ptr) return;

    BlockHeader* block = (BlockHeader*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    block->free = 1;

    // Coalesce adjacent free blocks
    BlockHeader* curr = free_list;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += BLOCK_HEADER_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void pool_destroy() {
    memory_pool = NULL;
    memory_pool_size = 0;
    free_list = NULL;
}
