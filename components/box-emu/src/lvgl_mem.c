// LVGL memory in PSRAM.
//
// LVGL is built with CONFIG_LV_USE_CUSTOM_MALLOC (see sdkconfig.defaults), so
// its allocator hooks are implemented here instead of by a fixed-size pool in
// internal RAM. Everything LVGL allocates (objects, styles, draw buffers, the
// rom list of the GUI, the pause menu, ...) goes to the PSRAM heap; only if
// PSRAM is exhausted does an allocation fall back to internal RAM.
//
// The display's DMA buffers are not affected: they are allocated by the BSP
// with the capabilities the LCD driver needs.
#include <esp_heap_caps.h>

#include "lvgl.h"

#define LVGL_PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LVGL_FALLBACK_CAPS (MALLOC_CAP_8BIT)

void lv_mem_init(void) {
  // The heaps are set up by ESP-IDF before app_main().
}

void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes) {
  // Not supported: the system heap is the pool.
  LV_UNUSED(mem);
  LV_UNUSED(bytes);
  return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool) { LV_UNUSED(pool); }

void *lv_malloc_core(size_t size) {
  void *p = heap_caps_malloc(size, LVGL_PSRAM_CAPS);
  if (p == NULL) {
    p = heap_caps_malloc(size, LVGL_FALLBACK_CAPS);
  }
  return p;
}

void *lv_realloc_core(void *p, size_t new_size) {
  void *new_p = heap_caps_realloc(p, new_size, LVGL_PSRAM_CAPS);
  if (new_p == NULL) {
    new_p = heap_caps_realloc(p, new_size, LVGL_FALLBACK_CAPS);
  }
  return new_p;
}

void lv_free_core(void *p) { heap_caps_free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) {
  // Report the PSRAM heap, which is where LVGL's memory lives.
  multi_heap_info_t info;
  heap_caps_get_info(&info, LVGL_PSRAM_CAPS);
  mon_p->total_size = info.total_free_bytes + info.total_allocated_bytes;
  mon_p->free_cnt = info.free_blocks;
  mon_p->free_size = info.total_free_bytes;
  mon_p->free_biggest_size = info.largest_free_block;
  mon_p->used_cnt = info.allocated_blocks;
  mon_p->max_used = mon_p->total_size - info.minimum_free_bytes;
  mon_p->used_pct = mon_p->total_size ? (uint8_t)(100 - (100ULL * info.total_free_bytes) / mon_p->total_size) : 0;
  mon_p->frag_pct = mon_p->free_size ? (uint8_t)(100 - (100ULL * info.largest_free_block) / mon_p->free_size) : 0;
}

lv_result_t lv_mem_test_core(void) {
  return heap_caps_check_integrity(LVGL_PSRAM_CAPS, false) ? LV_RESULT_OK : LV_RESULT_INVALID;
}
