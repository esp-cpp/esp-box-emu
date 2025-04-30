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
void* pool_alloc(size_t size);
void pool_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // POOL_ALLOCATOR_H
