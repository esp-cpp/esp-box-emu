#include "memory_census.hpp"

#include <algorithm>
#include <vector>

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "format.hpp"

MemSnapshot mem_snapshot() {
  MemSnapshot s;
  s.internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  s.internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  s.internal_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  s.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  s.psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  s.dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
  s.dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
  return s;
}

static long delta(size_t now, size_t before) {
  return (long)now - (long)before;
}

MemSnapshot print_mem_census(const char *phase, const MemSnapshot *baseline) {
  MemSnapshot s = mem_snapshot();
  fmt::print("[mem] phase={} internal: free={} largest={} min_free={} | psram: free={} largest={} | dma: free={} largest={}\n",
             phase, s.internal_free, s.internal_largest, s.internal_min_free,
             s.psram_free, s.psram_largest, s.dma_free, s.dma_largest);
  if (baseline) {
    // A non-zero free delta after a launch/quit cycle is a leak (or a lazily
    // created singleton); a shrinking largest block with an unchanged free
    // count is fragmentation.
    fmt::print("[mem] delta phase={} internal: free={:+} largest={:+} | psram: free={:+} largest={:+}\n",
               phase,
               delta(s.internal_free, baseline->internal_free),
               delta(s.internal_largest, baseline->internal_largest),
               delta(s.psram_free, baseline->psram_free),
               delta(s.psram_largest, baseline->psram_largest));
  }
  return s;
}

namespace {

struct RegionStats {
  intptr_t start{0};
  intptr_t end{0};
  size_t used{0};
  size_t free{0};
  size_t largest_free{0};
  size_t used_blocks{0};
};

struct BlockRec {
  const void *ptr;
  size_t size;
};

// The walker callback runs with the heap locked, so it must not allocate or
// print (printing may allocate): it only records into storage reserved up
// front, and everything is printed after the walk.
struct WalkState {
  size_t min_size{0};
  std::vector<RegionStats> regions; // one per internal heap region, in walk order
  std::vector<BlockRec> blocks;     // used blocks >= min_size (capped at capacity)
};

// Name of the task whose stack lives inside [ptr, ptr+size), if any.
const char *stack_owner(const std::vector<TaskStatus_t> &tasks, const void *ptr, size_t size) {
  const auto lo = reinterpret_cast<uintptr_t>(ptr);
  const auto hi = lo + size;
  for (const auto &t : tasks) {
    const auto base = reinterpret_cast<uintptr_t>(t.pxStackBase);
    if (base >= lo && base < hi)
      return t.pcTaskName;
  }
  return nullptr;
}

bool walk_cb(walker_heap_into_t heap, walker_block_info_t block, void *user) {
  auto *st = static_cast<WalkState *>(user);
  auto it = std::find_if(st->regions.begin(), st->regions.end(),
                         [&](const RegionStats &r) { return r.start == heap.start; });
  if (it == st->regions.end()) {
    if (st->regions.size() == st->regions.capacity())
      return true; // more regions than reserved: keep walking, just don't record
    st->regions.push_back(RegionStats{heap.start, heap.end});
    it = st->regions.end() - 1;
  }
  if (block.used) {
    it->used += block.size;
    it->used_blocks++;
    if (block.size >= st->min_size && st->blocks.size() < st->blocks.capacity())
      st->blocks.push_back(BlockRec{block.ptr, block.size});
  } else {
    it->free += block.size;
    it->largest_free = std::max(it->largest_free, block.size);
  }
  return true;
}

} // namespace

void print_mem_blocks(const char *phase, size_t min_size) {
  // Snapshot the task list first so stack blocks can be attributed. The
  // vectors live on the heap for a moment; they are freed before we return, so
  // the census it prints is unaffected apart from a few transient blocks.
  std::vector<TaskStatus_t> tasks(uxTaskGetNumberOfTasks() + 4);
  const UBaseType_t n = uxTaskGetSystemState(tasks.data(), tasks.size(), nullptr);
  tasks.resize(n);

  WalkState st{min_size, {}, {}};
  st.regions.reserve(8);
  st.blocks.reserve(min_size == SIZE_MAX ? 0 : 128);
  heap_caps_walk(MALLOC_CAP_INTERNAL, walk_cb, &st);

  for (const auto &b : st.blocks) {
    // A block that another registered heap lives inside is one of the
    // SPIRAM_MALLOC_RESERVE_INTERNAL pool chunks, not an allocation of its own.
    const auto lo = reinterpret_cast<uintptr_t>(b.ptr);
    const bool is_pool = std::any_of(st.regions.begin(), st.regions.end(), [&](const RegionStats &r) {
      return (uintptr_t)r.start >= lo && (uintptr_t)r.start < lo + b.size;
    });
    const char *owner = is_pool ? nullptr : stack_owner(tasks, b.ptr, b.size);
    fmt::print("[mem] block phase={} addr={} size={}{}{}\n", phase, b.ptr, b.size,
               is_pool ? " reserve_pool" : (owner ? " stack=" : ""), owner ? owner : "");
  }

  for (const auto &r : st.regions) {
    fmt::print("[mem] region phase={} start={:#x} size={} used={} used_blocks={} free={} largest={}\n",
               phase, r.start, (size_t)(r.end - r.start), r.used, r.used_blocks, r.free, r.largest_free);
  }

  // Stack high-water marks (bytes still unused at the deepest point so far):
  // a task whose mark stays large across a full play session has an oversized
  // stack that could be shrunk to give the heap back a contiguous block.
  for (const auto &t : tasks) {
    fmt::print("[mem] task phase={} name={} core={} prio={} stack_free_min={}\n", phase, t.pcTaskName,
               (int)t.xCoreID, (int)t.uxCurrentPriority,
               (size_t)t.usStackHighWaterMark * sizeof(StackType_t));
  }
}
