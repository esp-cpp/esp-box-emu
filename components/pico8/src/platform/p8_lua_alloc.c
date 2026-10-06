/* A Lua allocator for the ESP32-S3.
 *
 * The interpreter allocates and frees small objects constantly (TValues in
 * tables, closures, strings, upvalues); the system heap serves each one
 * through a lock and a general-purpose search, and on this board a string
 * creation measured ~12k cycles. Lua's lua_Alloc contract hands us the old
 * size on every free and realloc, so small blocks can come from size-class
 * free lists with no header and no locking (one Lua state, one task).
 * Blocks larger than the biggest class go to the heap as before.
 *
 * Chunks for the free lists are carved from PSRAM in 64KB slabs and returned
 * to the heap when the allocator is closed (femto8 closes the Lua state at
 * shutdown; the glue calls p8_lua_alloc_close() afterwards).
 */
#include "p8_lua_alloc.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

#define SLAB_BYTES (64 * 1024)
#define MAX_SLABS 128              /* 8MB of small objects at most */
#define NUM_CLASSES 16             /* 16, 32, ..., 256 bytes */
#define CLASS_STEP 16
#define MAX_SMALL (NUM_CLASSES * CLASS_STEP)

typedef struct free_node { struct free_node *next; } free_node_t;

static free_node_t *s_free[NUM_CLASSES];
static void *s_slabs[MAX_SLABS];
static int s_num_slabs;
static uint8_t *s_bump, *s_bump_end;   /* unallocated tail of the newest slab */
static size_t s_small_live, s_large_live;

static inline int class_of(size_t n) { return (int)((n + CLASS_STEP - 1) / CLASS_STEP) - 1; }

static void *small_alloc(size_t n)
{
    int c = class_of(n);
    free_node_t *node = s_free[c];
    if (node) {
        s_free[c] = node->next;
        return node;
    }
    size_t sz = (size_t)(c + 1) * CLASS_STEP;
    if (s_bump + sz > s_bump_end) {
        if (s_num_slabs >= MAX_SLABS)
            return NULL;
        void *slab = heap_caps_malloc(SLAB_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!slab)
            return NULL;
        s_slabs[s_num_slabs++] = slab;
        s_bump = (uint8_t *)slab;
        s_bump_end = s_bump + SLAB_BYTES;
    }
    void *p = s_bump;
    s_bump += sz;
    return p;
}

static void small_free(void *p, size_t n)
{
    int c = class_of(n);
    free_node_t *node = (free_node_t *)p;
    node->next = s_free[c];
    s_free[c] = node;
}

void *p8_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
    if (nsize == 0) {
        if (ptr) {
            if (osize <= MAX_SMALL) { small_free(ptr, osize); s_small_live -= osize; }
            else { free(ptr); s_large_live -= osize; }
        }
        return NULL;
    }
    if (ptr == NULL) {
        /* Lua passes the object type in osize for a fresh allocation; it is
         * not a size, so only nsize matters here */
        if (nsize <= MAX_SMALL) {
            void *p = small_alloc(nsize);
            if (p) s_small_live += nsize;
            return p;
        }
        void *p = malloc(nsize);
        if (p) s_large_live += nsize;
        return p;
    }
    /* realloc: same class stays put, otherwise move */
    if (osize <= MAX_SMALL && nsize <= MAX_SMALL && class_of(osize) == class_of(nsize)) {
        s_small_live += nsize - osize;
        return ptr;
    }
    if (osize > MAX_SMALL && nsize > MAX_SMALL) {
        void *p = realloc(ptr, nsize);
        if (p) s_large_live += nsize - osize;
        return p;
    }
    void *np = p8_lua_alloc(ud, NULL, 0, nsize);
    if (!np)
        return NULL;
    memcpy(np, ptr, osize < nsize ? osize : nsize);
    p8_lua_alloc(ud, ptr, osize, 0);
    return np;
}

void p8_lua_alloc_close(void)
{
    for (int i = 0; i < s_num_slabs; i++)
        heap_caps_free(s_slabs[i]);
    s_num_slabs = 0;
    s_bump = s_bump_end = NULL;
    memset(s_free, 0, sizeof(s_free));
    s_small_live = s_large_live = 0;
}

void p8_lua_alloc_stats(size_t *small_live, size_t *large_live, size_t *slab_bytes)
{
    if (small_live) *small_live = s_small_live;
    if (large_live) *large_live = s_large_live;
    if (slab_bytes) *slab_bytes = (size_t)s_num_slabs * SLAB_BYTES;
}
