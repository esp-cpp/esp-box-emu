#pragma once
//////////////////////////////////////////////////////////////////////
// TFE_ESPBOX: helpers for moving the engine's large statics into the
// emulator's shared memory system.
//
// A converted static keeps its name but becomes a pointer. The file that
// owns it defines an espbox_shared_<name>(bool alloc) hook that allocates it
// with shared_malloc() (zeroed) or clears the pointer; the hooks are called
// from darkforces_init_shared_memory() / darkforces_free_shared_memory().
// The memory itself is released by shared_mem_clear() when the game exits.
//////////////////////////////////////////////////////////////////////
#ifdef TFE_ESPBOX
#include <cstddef>
#include "shared_memory.h"

#define ESPBOX_SHARED_ALLOC(ptr, count) \
	((ptr) = alloc ? static_cast<decltype(ptr)>(shared_malloc(sizeof(*(ptr)) * (count))) : nullptr)
#endif
