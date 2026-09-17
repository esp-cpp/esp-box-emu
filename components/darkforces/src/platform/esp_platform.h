#pragma once
// Shared ESP32 platform helpers for the Dark Forces port.
#include <cstddef>
#include <cstdint>

namespace TFE_Memory
{
	// Serializes all access to the pool allocator (recursive).
	struct PoolLock
	{
		PoolLock();
		~PoolLock();
	};

	// While at least one scope is active on a task, allocations made by Dark
	// Forces code on that task go into the 4MB ROM block (see esp_alloc.cpp).
	struct DfAllocScope
	{
		DfAllocScope();
		~DfAllocScope();
	};

	// Temporarily sends Dark Forces allocations on this task to the PSRAM heap
	// (for objects that must outlive the session).
	struct DfHeapScope
	{
		DfHeapScope();
		~DfHeapScope();
	private:
		int m_saved;
	};

	// Start / end a session: the pool allocator manages the block while Dark Forces runs.
	// poolEnd() reports anything still allocated in the block.
	bool poolBegin(uint8_t* base, size_t size);
	void poolEnd();
	void getPoolStats(size_t* used, size_t* peak, size_t* overflowBytes);

	// Raw, thread safe block allocations (no heap fallback; null when the block is full).
	void* lockedPoolAlloc(size_t size);
	void lockedPoolFree(void* ptr);
	// Record an allocation that did not fit in the block and went to the PSRAM heap.
	void noteRegionOverflow(size_t size);

	// Bytes currently allocated through the memory regions, split by backing store.
	void getRegionStats(size_t* poolBytes, size_t* heapBytes, size_t* gameBytes, size_t* levelBytes);
	// Print the biggest region_alloc() callers (bytes currently allocated).
	void printRegionCallers();
}
