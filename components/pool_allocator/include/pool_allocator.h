#ifndef POOL_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// This file contains the declaration of a simple memory pool allocator. This
// memory pool is actually designed to be given a single (large) block of memory
// which it will then manage allocations/deallocations within

void pool_create(void* region, size_t size);
void pool_destroy();
int pool_contains(const void* ptr);
void* pool_alloc(size_t size);
void pool_free(void* ptr);
// Usable size of an allocated block (0 if ptr is not in the pool).
size_t pool_block_size(const void* ptr);
// Visit every block (allocated or free) in address order.
void pool_walk(void (*callback)(void* ptr, size_t size, int used, void* user), void* user);

#ifdef __cplusplus
}
#endif

#endif // POOL_ALLOCATOR_H
