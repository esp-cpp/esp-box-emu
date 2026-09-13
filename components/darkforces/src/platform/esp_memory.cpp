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

#include <cassert>
#include <cstdlib>
#include <cstring>

struct MemoryBlock
{
	MemoryBlock* prev;
	MemoryBlock* next;
	u32 size;
	u32 pad;	// keep the payload 16-byte aligned
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
	// The 4MB ROM block owned by BoxEmu is unused while Dark Forces runs, so it
	// serves as the primary pool (see init_darkforces()); the PSRAM heap is the fallback.
	static void* regionMalloc(size_t size)
	{
		void* mem = pool_alloc(size);
		if (!mem)
		{
			mem = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		}
		if (!mem)
		{
			mem = malloc(size);
		}
		return mem;
	}

	static void regionFree(void* mem)
	{
		if (pool_contains(mem)) { pool_free(mem); }
		else { free(mem); }
	}

	static MemoryBlock* newBlock(MemoryRegion* region, u32 size)
	{
		MemoryBlock* node = (MemoryBlock*)regionMalloc(sizeof(MemoryBlock) + size);
		if (!node) { return nullptr; }

		node->size = size;
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
		regionFree(node);
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
		return region;
	}

	void region_clear(MemoryRegion* region)
	{
		assert(region);
		MemoryBlock* node = region->head;
		while (node)
		{
			MemoryBlock* next = node->next;
			regionFree(node);
			node = next;
		}
		region->head = nullptr;
		region->tail = nullptr;
		region->used = 0;
		region->count = 0;
	}

	void region_destroy(MemoryRegion* region)
	{
		if (!region) { return; }
		region_clear(region);
		free(region);
	}

	void* region_alloc(MemoryRegion* region, size_t size)
	{
		assert(region);
		if (!region || size == 0) { return nullptr; }

		MemoryBlock* node = newBlock(region, (u32)size);
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
		assert(region);
		if (!ptr) { return region_alloc(region, size); }
		if (size == 0) { return nullptr; }

		MemoryBlock* node = blockFromPtr(ptr);
		const u32 prevSize = node->size;
		if (prevSize >= size)
		{
			return ptr;
		}

		MemoryBlock* newNode = newBlock(region, (u32)size);
		if (!newNode) { return nullptr; }
		void* newMem = ptrFromBlock(newNode);
		memcpy(newMem, ptr, prevSize);
		freeBlock(region, node);
		return newMem;
	}

	void region_free(MemoryRegion* region, void* ptr)
	{
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
