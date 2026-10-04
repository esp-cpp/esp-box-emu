#ifndef MALLOC_H
#define MALLOC_H
#ifndef __psyz

#include <stddef.h>

/* Standard malloc family (libc2) */

/**
 * @brief Allocate main memory
 *
 * Allocates a block of s bytes from the memory heap.
 * Note that the memory heap is defined as follows when the user program is
 * activated: Bottom address: top address of module + 4. Top address: available
 * memory -32KB.
 *
 * This function has a bug whereby the area is not completely released in
 * free(). This function can be replaced by malloc2() or malloc3() from libapi.
 *
 * @param s Number of bytes to be allocated
 * @return A pointer to the secured memory block. If allocation fails, NULL is
 * returned.
 */
void* malloc(size_t s);

/**
 * @brief Allocate main memory
 *
 * Allocates a memory area of n blocks of s bytes each from the heap and
 * initializes it to 0.
 *
 * @param n Number of blocks
 * @param s Size of block
 * @return A pointer to the memory area allocated. If the function fails, it
 * returns NULL.
 */
void* calloc(size_t n, size_t s);

/**
 * @brief Change heap memory allocations
 *
 * Takes a previously allocated block and contracts it or expands it to s bytes.
 * If block is NULL, this function works in the same way as malloc.
 *
 * @param block Pointer to a block allocated by a function such as malloc()
 * @param s New size
 * @return The address of the reallocated block. May be different from the old
 * address. If the allocation fails, the function returns NULL, and the old
 * block is not released.
 */
void* realloc(void* block, size_t s);

/**
 * @brief Release an allocated memory block
 *
 * Releases a memory block that was allocated by calloc(), malloc() or
 * realloc().
 *
 * @param block Pointer to a memory block allocated by a function such as
 * malloc()
 */
void free(void* block);

/* malloc2 family (introduced in 3.6) */

/**
 * @brief Initialize heap area
 *
 * Initializes a heap area of size bytes. (Since there is overhead, the entire
 * size in bytes cannot be used.) After calling this function, the library
 * memory routines in the "malloc2" group (malloc2(), free2(), etc.) are usable.
 * This routine fixes a bug in InitHeap() but has larger program size since this
 * is a memory resident function. If several executions of this function
 * overlap, the previous memory control information is lost.
 *
 * @param head Pointer to heap start address
 * @param size Heap size (a multiple of 4, in bytes)
 */
void InitHeap2(void* head, long size);

/**
 * @brief Allocate main memory
 *
 * Allocates s bytes of memory block from the heap memory. InitHeap2() must be
 * executed in advance.
 *
 * @param s Size of memory block to be allocated
 * @return A pointer to the allocated memory block. On failure, NULL is
 * returned.
 */
void* malloc2(size_t s);

/**
 * @brief Allocate a block in main memory
 *
 * Allocates a block of n*s bytes in the heap memory and initializes it to 0.
 * Corresponds to InitHeap2().
 *
 * @param n Number of partitions
 * @param s Size of one partition
 * @return Pointer to the allocated memory block. If allocation fails, NULL is
 * returned.
 */
void* calloc2(size_t n, size_t s);

/**
 * @brief Change a block's memory allocation
 *
 * Changes the size of the memory block previously allocated to s bytes.
 * Same as malloc2() when block is NULL. Corresponds to InitHeap2().
 *
 * @param block Area to be reallocated
 * @param s Size of area to be reallocated
 * @return A pointer to the reallocated memory block. The address may be from
 * the original. If reallocation fails, NULL is returned, and the original block
 * is not released.
 */
void* realloc2(void* block, size_t s);

/**
 * @brief Free allocated memory blocks
 *
 * Releases a memory block that was allocated by calloc2(), malloc2(), or
 * realloc2(). Corresponds to InitHeap2().
 *
 * @param block Area to be released
 */
void free2(void* block);

/* malloc3 family (introduced in 4.0) */

/**
 * @brief Initialize heap area
 *
 * Initializes a heap area of size bytes. If size is not divisible by 8, the
 * remainder after dividing by 8 is discarded and isn't allocated. (Since there
 * is overhead, the entire size in bytes cannot be used.) After calling this
 * function, the library memory routines in the "malloc3" group (malloc3(),
 * free3(), etc.) are usable. This function is a higher speed than the "malloc2"
 * system and is smaller in size. If several executions of this function
 * overlap, the previous memory control information is lost.
 *
 * @param head Pointer to heap start address
 * @param size Heap size (a multiple of 8, in bytes)
 */
void InitHeap3(void* head, long size);

/**
 * @brief Allocate main memory
 *
 * Allocates s bytes of memory block from the heap memory. InitHeap3() must be
 * executed in advance.
 *
 * @param s Size of memory block to be allocated
 * @return A pointer to the allocated memory block. If allocation fails, NULL is
 * returned.
 */
void* malloc3(size_t s);

/**
 * @brief Allocate a block in main memory
 *
 * Allocates a block of n*s bytes in the heap memory and initializes it to 0.
 * Corresponds to InitHeap3().
 *
 * @param n Number of partitions
 * @param s Size of one partition
 * @return A pointer to the allocated memory block. If allocation fails, NULL is
 * returned.
 */
void* calloc3(size_t n, size_t s);

/**
 * @brief Change a block's memory allocation
 *
 * Changes the size of the memory block previously allocated to s bytes.
 * When block is NULL, it operates in the same way as malloc3().
 *
 * @param block Area to be reallocated
 * @param s Size of area to be reallocated
 * @return A pointer to the reallocated block address; this address may be
 * different from the original. If reallocation fails, NULL is returned, and the
 * original block is not released. NULL is also returned if s is 0.
 */
void* realloc3(void* block, size_t s);

/**
 * @brief Free allocated memory blocks
 *
 * Releases a memory block that was allocated by calloc3(), malloc3(), or
 * realloc3().
 *
 * @param block Area to be released
 */
void free3(void* block);

#endif
#endif
