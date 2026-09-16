// ESP32 (esp-box-emu) memory regions for The Force Engine.
//
// The desktop implementation reserves large fixed blocks (8MB per region) which
// does not fit the ESP32's PSRAM budget. This version simply tracks each
// allocation in an intrusive list so a region can still be cleared in one call;
// the memory itself comes from the PSRAM heap.
#include <TFE_Memory/memoryRegion.h>
#include <TFE_System/system.h>

#include <esp_heap_caps.h>
#include "pool_allocator.h"
#include "esp_platform.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cassert>
#include <cstdlib>
#include <cstring>

struct MemoryBlock
{
	MemoryBlock* prev;
	MemoryBlock* next;
	u32 size;
	u32 caller;	// return address of the region_alloc() caller (memory attribution)
};

struct MemoryRegion
{
	char name[32];
	MemoryBlock* head;
	MemoryBlock* tail;
	u64 used;
	u64 count;
};

namespace TFE_Memory
{
	// Regions are used from the game thread and from the iMuse/MIDI thread, and the
	// underlying pool allocator is not thread safe: serialize all region operations.
	static SemaphoreHandle_t s_lock = nullptr;
	struct RegionLock
	{
		RegionLock()
		{
			if (!s_lock) { s_lock = xSemaphoreCreateRecursiveMutex(); }
			xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
		}
		~RegionLock() { xSemaphoreGiveRecursive(s_lock); }
	};

	void* lockedPoolAlloc(size_t size)
	{
		RegionLock lock;
		return pool_alloc(size);
	}

	void lockedPoolFree(void* ptr)
	{
		RegionLock lock;
		if (pool_contains(ptr)) { pool_free(ptr); }
	}

	// The 4MB ROM block owned by BoxEmu is unused while Dark Forces runs, so it
	// serves as the primary pool (see init_darkforces()); the PSRAM heap is the fallback.
	static size_t s_poolBytes = 0, s_heapBytes = 0;

	// Per-caller attribution of region memory (bytes currently allocated).
	struct CallerStat { u32 pc; size_t bytes; size_t count; };
	static CallerStat s_callers[128];
	static CallerStat* callerStat(u32 pc)
	{
		for (auto& c : s_callers)
		{
			if (c.pc == pc) { return &c; }
			if (c.pc == 0) { c.pc = pc; c.bytes = 0; c.count = 0; return &c; }
		}
		return nullptr;
	}
	static void callerAdd(u32 pc, size_t bytes)
	{
		CallerStat* c = callerStat(pc);
		if (c) { c->bytes += bytes; c->count++; }
	}
	static void callerSub(u32 pc, size_t bytes)
	{
		CallerStat* c = callerStat(pc);
		if (c && c->count) { c->bytes -= bytes; c->count--; }
	}
	static MemoryRegion* s_regions[2] = { nullptr, nullptr };	// first two regions created: game, level

	static void* regionMalloc(size_t size)
	{
		void* mem = pool_alloc(size);
		if (mem) { s_poolBytes += size; return mem; }
		mem = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (!mem) { mem = malloc(size); }
		if (mem) { s_heapBytes += size; }
		return mem;
	}

	static void regionFree(void* mem, size_t size)
	{
		if (pool_contains(mem)) { pool_free(mem); s_poolBytes -= size; }
		else { free(mem); s_heapBytes -= size; }
	}

	static MemoryBlock* newBlock(MemoryRegion* region, u32 size, u32 caller)
	{
		MemoryBlock* node = (MemoryBlock*)regionMalloc(sizeof(MemoryBlock) + size);
		if (!node) { return nullptr; }

		node->size = size;
		node->caller = caller;
		callerAdd(caller, size);
		node->next = nullptr;
		node->prev = region->tail;
		if (region->tail) { region->tail->next = node; }
		else { region->head = node; }
		region->tail = node;
		region->used += size;
		region->count++;
		return node;
	}

	static void freeBlock(MemoryRegion* region, MemoryBlock* node)
	{
		if (node->prev) { node->prev->next = node->next; }
		else { region->head = node->next; }
		if (node->next) { node->next->prev = node->prev; }
		else { region->tail = node->prev; }
		region->used -= node->size;
		region->count--;
		callerSub(node->caller, node->size);
		regionFree(node, sizeof(MemoryBlock) + node->size);
	}

	static inline MemoryBlock* blockFromPtr(void* ptr)
	{
		return ((MemoryBlock*)ptr) - 1;
	}

	static inline void* ptrFromBlock(MemoryBlock* node)
	{
		return (void*)(node + 1);
	}

	MemoryRegion* region_create(const char* name, size_t blockSize, size_t maxSize)
	{
		RegionLock lock;
		assert(name);
		if (!name || !blockSize) { return nullptr; }

		MemoryRegion* region = (MemoryRegion*)malloc(sizeof(MemoryRegion));
		if (!region)
		{
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Failed to allocate region '%s'.", name);
			return nullptr;
		}

		strncpy(region->name, name, sizeof(region->name) - 1);
		region->name[sizeof(region->name) - 1] = 0;
		region->head = nullptr;
		region->tail = nullptr;
		region->used = 0;
		region->count = 0;
		if (!s_regions[0]) { s_regions[0] = region; }
		else if (!s_regions[1]) { s_regions[1] = region; }
		return region;
	}

	void printRegionCallers()
	{
		RegionLock lock;
		// Sort a copy by bytes and print the top entries; symbolize the pcs with addr2line.
		CallerStat copy[128];
		memcpy(copy, s_callers, sizeof(copy));
		for (int i = 0; i < 24; i++)
		{
			int best = -1;
			for (int j = 0; j < 128; j++) { if (copy[j].pc && copy[j].bytes && (best < 0 || copy[j].bytes > copy[best].bytes)) { best = j; } }
			if (best < 0) { break; }
			printf("[DarkForces]   region caller 0x%08x: %u KB in %u blocks\n", (unsigned)copy[best].pc, (unsigned)(copy[best].bytes / 1024), (unsigned)copy[best].count);
			copy[best].bytes = 0;
		}
	}

	void getRegionStats(size_t* poolBytes, size_t* heapBytes, size_t* gameBytes, size_t* levelBytes)
	{
		RegionLock lock;
		*poolBytes = s_poolBytes;
		*heapBytes = s_heapBytes;
		*gameBytes = s_regions[0] ? (size_t)s_regions[0]->used : 0;
		*levelBytes = s_regions[1] ? (size_t)s_regions[1]->used : 0;
	}

	void region_clear(MemoryRegion* region)
	{
		RegionLock lock;
		assert(region);
		MemoryBlock* node = region->head;
		while (node)
		{
			MemoryBlock* next = node->next;
			callerSub(node->caller, node->size);
			regionFree(node, sizeof(MemoryBlock) + node->size);
			node = next;
		}
		region->head = nullptr;
		region->tail = nullptr;
		region->used = 0;
		region->count = 0;
	}

	void region_destroy(MemoryRegion* region)
	{
		RegionLock lock;
		if (!region) { return; }
		region_clear(region);
		if (s_regions[0] == region) { s_regions[0] = nullptr; }
		if (s_regions[1] == region) { s_regions[1] = nullptr; }
		free(region);
	}

	void* region_alloc(MemoryRegion* region, size_t size)
	{
		RegionLock lock;
		assert(region);
		if (!region || size == 0) { return nullptr; }

		MemoryBlock* node = newBlock(region, (u32)size, (u32)__builtin_return_address(0));
		if (node)
		{
			return ptrFromBlock(node);
		}

		TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Failed to allocate %u bytes in region '%s' (used %u in %u blocks).",
			(u32)size, region->name, (u32)region->used, (u32)region->count);
		return nullptr;
	}

	void* region_realloc(MemoryRegion* region, void* ptr, size_t size)
	{
		RegionLock lock;
		assert(region);
		if (!ptr) { return region_alloc(region, size); }
		if (size == 0) { return nullptr; }

		MemoryBlock* node = blockFromPtr(ptr);
		const u32 prevSize = node->size;
		if (prevSize >= size)
		{
			return ptr;
		}

		MemoryBlock* newNode = newBlock(region, (u32)size, node->caller);
		if (!newNode) { return nullptr; }
		void* newMem = ptrFromBlock(newNode);
		memcpy(newMem, ptr, prevSize);
		freeBlock(region, node);
		return newMem;
	}

	void region_free(MemoryRegion* region, void* ptr)
	{
		RegionLock lock;
		if (!ptr || !region) { return; }
		freeBlock(region, blockFromPtr(ptr));
	}

	size_t region_getMemoryUsed(MemoryRegion* region)
	{
		return region ? region->used : 0;
	}

	void region_getBlockInfo(MemoryRegion* region, size_t* blockCount, size_t* blockSize)
	{
		*blockCount = region ? region->count : 0;
		*blockSize = 1;
	}

	size_t region_getMemoryCapacity(MemoryRegion* region)
	{
		return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) + region_getMemoryUsed(region);
	}

	RelativePointer region_getRelativePointer(MemoryRegion* region, void* ptr)
	{
		return NULL_RELATIVE_POINTER;
	}

	void* region_getRealPointer(MemoryRegion* region, RelativePointer ptr)
	{
		return nullptr;
	}

	bool region_serializeToDisk(MemoryRegion* region, FileStream* file)
	{
		return false;
	}

	MemoryRegion* region_restoreFromDisk(MemoryRegion* region, FileStream* file)
	{
		return nullptr;
	}

	void region_test()
	{
	}
}
