// Dark Forces memory: everything the engine allocates lives in BoxEmu's 4MB ROM block.
//
// Dark Forces loads its data from the SD card, so the ROM block that the other
// cores use for ROM images is free while it runs. The block is managed by the
// pool allocator and holds:
//  - the engine's memory regions (esp_memory.cpp),
//  - its large static buffers (see TFE_System/espboxShared.h),
//  - and every malloc/calloc/realloc/strdup/new made by Dark Forces code: after
//    libdarkforces.a is built, those symbols are renamed to the df_* functions
//    below (see CMakeLists.txt and alloc_redirect.syms).
//
// Rules that keep this safe:
//  - The block is only used while a DfAllocScope is active on the calling task
//    (the game entry points in darkforces.cpp and the engine's own threads).
//    Template code shared with other components can end up calling df_*, and
//    those calls must not put memory into the block.
//  - free() and realloc() are wrapped for the whole firmware (--wrap), so a
//    block pointer is handled here no matter which code releases it.
//  - Each df_* allocation carries a header with a per-session magic value;
//    frees of stale pointers (from an earlier session, after the block was
//    handed back to the other cores) are ignored.
//  - Nothing may outlive a session in the block. darkforces_pool_end() lists
//    any allocation that is still live so it can be released at shutdown.
#include "esp_platform.h"
#include "pool_allocator.h"

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void __real_free(void* ptr);
extern "C" void* __real_realloc(void* ptr, size_t size);

namespace
{
	struct AllocHeader
	{
		uint32_t magic;
		uint32_t size;		// requested size
		uint32_t caller;	// return address of the allocating call
		uint32_t offset;	// distance from the pool block to the user pointer
	};
	constexpr uint32_t HEADER_MAGIC = 0xDFA11C00;

	SemaphoreHandle_t s_lock = nullptr;
	uint8_t* s_base = nullptr;		// the block stays known after the session ends (stale frees)
	size_t s_size = 0;
	bool s_active = false;
	uint32_t s_generation = 0;

	size_t s_used = 0;				// bytes allocated in the block
	size_t s_peak = 0;
	size_t s_overflowCount = 0;		// allocations that did not fit and went to the PSRAM heap
	size_t s_overflowBytes = 0;
	size_t s_staleFrees = 0;

	thread_local int t_scope = 0;

	inline uint32_t sessionMagic() { return HEADER_MAGIC ^ s_generation; }

	// The block start itself is excluded: no allocation starts there, and BoxEmu frees
	// the whole block with free(romdata).
	inline bool inBlock(const void* ptr)
	{
		return s_base && (const uint8_t*)ptr > s_base && (const uint8_t*)ptr < s_base + s_size;
	}

	void* poolAllocLocked(size_t size)
	{
		if (!s_active) { return nullptr; }
		void* mem = pool_alloc(size);
		if (mem)
		{
			s_used += pool_block_size(mem);
			if (s_used > s_peak) { s_peak = s_used; }
		}
		return mem;
	}

	void poolFreeLocked(void* mem)
	{
		s_used -= pool_block_size(mem);
		pool_free(mem);
	}

	void noteOverflow(size_t size)
	{
		if (!s_overflowCount)
		{
			printf("[DarkForces] WARNING: the 4MB ROM block is full (%u KB used); allocating %u B from the PSRAM heap\n",
				(unsigned)(s_used / 1024), (unsigned)size);
		}
		s_overflowCount++;
		s_overflowBytes += size;
	}

	// Returns the header of a live allocation from this session, or null.
	AllocHeader* liveHeader(void* ptr)
	{
		if (!s_active) { return nullptr; }
		AllocHeader* header = ((AllocHeader*)ptr) - 1;
		if ((uint8_t*)header < s_base || header->magic != sessionMagic()) { return nullptr; }
		return header;
	}

	void* dfAlloc(size_t size, uintptr_t caller)
	{
		if (t_scope > 0 && s_active)
		{
			TFE_Memory::PoolLock lock;
			// Keep malloc's 8 byte alignment in front of the header.
			const size_t total = size + sizeof(AllocHeader) + 8;
			void* mem = poolAllocLocked(total);
			if (mem)
			{
				uintptr_t user = ((uintptr_t)mem + sizeof(AllocHeader) + 7) & ~(uintptr_t)7;
				AllocHeader* header = ((AllocHeader*)user) - 1;
				header->magic = sessionMagic();
				header->size = (uint32_t)size;
				header->caller = (uint32_t)caller;
				header->offset = (uint32_t)(user - (uintptr_t)mem);
				return (void*)user;
			}
			noteOverflow(size);
		}
		return heap_caps_malloc(size ? size : 1, MALLOC_CAP_DEFAULT);
	}

	void dfFree(void* ptr)
	{
		if (!ptr) { return; }
		if (!inBlock(ptr))
		{
			__real_free(ptr);
			return;
		}
		TFE_Memory::PoolLock lock;
		AllocHeader* header = liveHeader(ptr);
		if (!header)
		{
			s_staleFrees++;
			return;
		}
		header->magic = 0;
		poolFreeLocked((uint8_t*)ptr - header->offset);
	}

	void* dfRealloc(void* ptr, size_t size, uintptr_t caller)
	{
		if (!ptr) { return dfAlloc(size, caller); }
		if (size == 0)
		{
			dfFree(ptr);
			return nullptr;
		}
		if (!inBlock(ptr)) { return __real_realloc(ptr, size); }

		size_t oldSize = 0;
		{
			TFE_Memory::PoolLock lock;
			AllocHeader* header = liveHeader(ptr);
			if (!header)
			{
				printf("[DarkForces] ERROR: realloc of a stale block pointer %p\n", ptr);
				s_staleFrees++;
				return dfAlloc(size, caller);
			}
			oldSize = header->size;
		}
		if (size <= oldSize) { return ptr; }
		void* mem = dfAlloc(size, caller);
		if (!mem) { return nullptr; }
		memcpy(mem, ptr, oldSize);
		dfFree(ptr);
		return mem;
	}

	struct CensusState
	{
		size_t liveCount;
		size_t liveBytes;
		size_t rawCount;
		size_t rawBytes;
		size_t printed;
	};

	void censusVisit(void* mem, size_t size, int used, void* user)
	{
		if (!used) { return; }
		CensusState* state = (CensusState*)user;
		uintptr_t userPtr = ((uintptr_t)mem + sizeof(AllocHeader) + 7) & ~(uintptr_t)7;
		const AllocHeader* header = ((AllocHeader*)userPtr) - 1;
		if (size >= sizeof(AllocHeader) + 8 && header->magic == sessionMagic())
		{
			state->liveCount++;
			state->liveBytes += header->size;
			if (state->printed++ < 32)
			{
				printf("[DarkForces]   still allocated: %u B from 0x%08x\n", (unsigned)header->size, (unsigned)header->caller);
			}
		}
		else
		{
			state->rawCount++;
			state->rawBytes += size;
			if (state->printed++ < 32)
			{
				// Region allocations start with a MemoryBlock header (see esp_memory.cpp):
				// prev, next, size, caller, owner.
				const uint32_t* words = (const uint32_t*)mem;
				const bool regionBlock = size >= 20 && words[2] + 20 <= size && words[2] + 20 + 16 > size &&
					words[3] >= 0x42000000 && words[3] < 0x44000000;
				if (regionBlock)
				{
					printf("[DarkForces]   still allocated: region block of %u B from 0x%08x\n", (unsigned)words[2], (unsigned)words[3]);
				}
				else if (size >= 16)
				{
					printf("[DarkForces]   still allocated: raw block of %u B at %p (%08x %08x %08x %08x)\n", (unsigned)size, mem,
						(unsigned)words[0], (unsigned)words[1], (unsigned)words[2], (unsigned)words[3]);
				}
				else
				{
					printf("[DarkForces]   still allocated: raw block of %u B at %p\n", (unsigned)size, mem);
				}
			}
		}
	}
}

namespace TFE_Memory
{
	PoolLock::PoolLock()
	{
		if (!s_lock) { s_lock = xSemaphoreCreateRecursiveMutex(); }
		xSemaphoreTakeRecursive(s_lock, portMAX_DELAY);
	}

	PoolLock::~PoolLock()
	{
		xSemaphoreGiveRecursive(s_lock);
	}

	DfAllocScope::DfAllocScope() { t_scope++; }
	DfAllocScope::~DfAllocScope() { t_scope--; }

	DfHeapScope::DfHeapScope() : m_saved(t_scope) { t_scope = 0; }
	DfHeapScope::~DfHeapScope() { t_scope = m_saved; }

	void* lockedPoolAlloc(size_t size)
	{
		PoolLock lock;
		return poolAllocLocked(size);
	}

	void lockedPoolFree(void* ptr)
	{
		PoolLock lock;
		if (s_active && ptr && pool_contains(ptr)) { poolFreeLocked(ptr); }
	}

	void noteRegionOverflow(size_t size)
	{
		PoolLock lock;
		noteOverflow(size);
	}

	bool poolBegin(uint8_t* base, size_t size)
	{
		PoolLock lock;
		if (!base || size < 1024) { return false; }
		pool_create(base, size);
		printf("[DarkForces] 4MB block: %p - %p\n", (void*)base, (void*)(base + size));
		s_base = base;
		s_size = size;
		s_active = true;
		s_generation++;
		s_used = 0;
		s_peak = 0;
		s_overflowCount = 0;
		s_overflowBytes = 0;
		s_staleFrees = 0;
		return true;
	}

	void poolEnd()
	{
		PoolLock lock;
		if (!s_active) { return; }
		CensusState state = {};
		pool_walk(censusVisit, &state);
		printf("[DarkForces] 4MB block at exit: peak %u KB, %u allocations (%u KB) and %u raw blocks (%u KB) still live, %u overflow allocations (%u KB), %u stale frees\n",
			(unsigned)(s_peak / 1024), (unsigned)state.liveCount, (unsigned)(state.liveBytes / 1024),
			(unsigned)state.rawCount, (unsigned)(state.rawBytes / 1024),
			(unsigned)s_overflowCount, (unsigned)(s_overflowBytes / 1024), (unsigned)s_staleFrees);
		s_active = false;
		pool_destroy();
	}

	void getPoolStats(size_t* used, size_t* peak, size_t* overflowBytes)
	{
		PoolLock lock;
		*used = s_used;
		*peak = s_peak;
		*overflowBytes = s_overflowBytes;
	}
}

// Engine buffers (espboxShared.h): zeroed, block only.
extern "C" void* espbox_pool_calloc(size_t size)
{
	void* mem = TFE_Memory::lockedPoolAlloc(size);
	if (mem) { memset(mem, 0, size); }
	return mem;
}

extern "C" void espbox_pool_free(void* ptr)
{
	TFE_Memory::lockedPoolFree(ptr);
}

// Replacements for the allocation functions referenced by libdarkforces.a.
extern "C"
{
	__attribute__((noinline)) void* df_malloc(size_t size)
	{
		return dfAlloc(size, (uintptr_t)__builtin_return_address(0));
	}

	__attribute__((noinline)) void* df_calloc(size_t count, size_t size)
	{
		if (size && count > SIZE_MAX / size) { return nullptr; }
		void* mem = dfAlloc(count * size, (uintptr_t)__builtin_return_address(0));
		if (mem) { memset(mem, 0, count * size); }
		return mem;
	}

	__attribute__((noinline)) void* df_realloc(void* ptr, size_t size)
	{
		return dfRealloc(ptr, size, (uintptr_t)__builtin_return_address(0));
	}

	void df_free(void* ptr)
	{
		dfFree(ptr);
	}

	__attribute__((noinline)) char* df_strdup(const char* str)
	{
		const size_t size = strlen(str) + 1;
		char* mem = (char*)dfAlloc(size, (uintptr_t)__builtin_return_address(0));
		if (mem) { memcpy(mem, str, size); }
		return mem;
	}

	// operator new / new[] (exceptions are disabled: abort like the default operator new).
	__attribute__((noinline)) void* df_new(size_t size)
	{
		void* mem = dfAlloc(size, (uintptr_t)__builtin_return_address(0));
		if (!mem)
		{
			printf("[DarkForces] out of memory in operator new (%u B)\n", (unsigned)size);
			abort();
		}
		return mem;
	}

	__attribute__((noinline)) void* df_new_nothrow(size_t size, void* /*nothrow*/)
	{
		return dfAlloc(size, (uintptr_t)__builtin_return_address(0));
	}

	void df_delete(void* ptr)
	{
		dfFree(ptr);
	}

	void df_delete_sized(void* ptr, size_t /*size*/)
	{
		dfFree(ptr);
	}

	// Array forms (objcopy needs a distinct target symbol for each renamed symbol).
	__attribute__((noinline)) void* df_new_array(size_t size)
	{
		void* mem = dfAlloc(size, (uintptr_t)__builtin_return_address(0));
		if (!mem)
		{
			printf("[DarkForces] out of memory in operator new[] (%u B)\n", (unsigned)size);
			abort();
		}
		return mem;
	}

	__attribute__((noinline)) void* df_new_array_nothrow(size_t size, void* /*nothrow*/)
	{
		return dfAlloc(size, (uintptr_t)__builtin_return_address(0));
	}

	void df_delete_array(void* ptr)
	{
		dfFree(ptr);
	}

	void df_delete_array_sized(void* ptr, size_t /*size*/)
	{
		dfFree(ptr);
	}

	// Firmware-wide wrappers (-Wl,--wrap=free,--wrap=realloc): route block pointers here.
	void __wrap_free(void* ptr)
	{
		if (inBlock(ptr)) { dfFree(ptr); }
		else { __real_free(ptr); }
	}

	void* __wrap_realloc(void* ptr, size_t size)
	{
		if (inBlock(ptr)) { return dfRealloc(ptr, size, (uintptr_t)__builtin_return_address(0)); }
		return __real_realloc(ptr, size);
	}
}
