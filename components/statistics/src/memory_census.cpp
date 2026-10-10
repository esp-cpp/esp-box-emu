#include "memory_census.hpp"

#include <esp_heap_caps.h>

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
