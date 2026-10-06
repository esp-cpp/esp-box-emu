#pragma once
/* esp-box-emu: a pooled lua_Alloc for femto8's Lua state (see p8_lua_alloc.c) */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *p8_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize);
/* release the pools; only after the Lua state that used them is closed */
void p8_lua_alloc_close(void);
void p8_lua_alloc_stats(size_t *small_live, size_t *large_live, size_t *slab_bytes);

#ifdef __cplusplus
}
#endif
