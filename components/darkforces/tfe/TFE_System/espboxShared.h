#pragma once
//////////////////////////////////////////////////////////////////////
// TFE_ESPBOX: the engine's large statics live in BoxEmu's 4MB ROM block.
//
// A converted static keeps its name but becomes a pointer. The file that
// owns it defines an espbox_shared_<name>(bool alloc) hook that allocates it
// (zeroed) from the block, or releases it and clears the pointer; the hooks
// are called from darkforces_init_shared_memory() /
// darkforces_free_shared_memory(). See src/platform/esp_alloc.cpp.
//////////////////////////////////////////////////////////////////////
#ifdef TFE_ESPBOX
#include <cstddef>

extern "C" void* espbox_pool_calloc(size_t size);
extern "C" void espbox_pool_free(void* ptr);

// Set when any hook's allocation fails; checked by darkforces_init_shared_memory().
extern bool g_espboxSharedAllocFailed;

#define ESPBOX_SHARED_ALLOC(ptr, count) \
	do { \
		if (alloc) { \
			(ptr) = static_cast<decltype(ptr)>(espbox_pool_calloc(sizeof(*(ptr)) * (count))); \
			if (!(ptr)) { g_espboxSharedAllocFailed = true; } \
		} else { \
			espbox_pool_free(static_cast<void*>(ptr)); \
			(ptr) = nullptr; \
		} \
	} while (0)
#endif
