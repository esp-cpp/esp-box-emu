#pragma once
// Shared ESP32 platform helpers for the Dark Forces port.
#include <cstddef>

namespace TFE_Memory
{
	// Thread safe access to the emulator's pool allocator (used by the memory
	// regions, the renderer display buffers and the platform glue).
	void* lockedPoolAlloc(size_t size);
	void lockedPoolFree(void* ptr);
	// Bytes currently allocated through the memory regions, split by backing store.
	void getRegionStats(size_t* poolBytes, size_t* heapBytes, size_t* gameBytes, size_t* levelBytes);
	// Print the biggest region_alloc() callers (bytes currently allocated).
	void printRegionCallers();
}
