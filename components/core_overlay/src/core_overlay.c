// The game cores' code overlay (overlay.ld, include/core_overlay.h): every
// core's code is linked at one instruction-bus address; the core a game needs
// is copied from its slot in the flash store (src/store.c) into a PSRAM block
// that is mapped at that address for the game.
//
// The ESP32-S3's MMU entry for a page serves the instruction bus (0x42...) and
// the data bus (0x3C...) alike, and a PSRAM page can be mapped at two
// addresses: the block comes from the PSRAM heap (its data-bus address is
// where the code is written) and esp_mmu_map_virt() maps the same pages at
// the overlay address, executable. Written through the data cache, the code
// is written back to PSRAM and the instruction cache (one, for both CPUs)
// drops the overlay range before the code runs.
#include "core_overlay.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mmu_map.h"
#include "esp_timer.h"

static const char *TAG = "core_overlay";

#define PAGE 0x10000u
#define MAGIC 0x4C564F43u // "COVL" (tools/core_overlay.py)

typedef void (*ctor_fn)(void);

// linker symbols (overlay.ld): the overlay range, and per core its code size
// (the symbol's value) and its constructor table (at the overlay address)
extern uint8_t _core_ovl_start[], _core_ovl_end[];
#define CORE_OVL_SYMBOLS(name)                                                                                        \
  extern const char _core_ovl_##name##_size[];                                                                         \
  extern const ctor_fn _core_ovl_ctors_##name##_start[], _core_ovl_ctors_##name##_end[];
CORE_OVL_SYMBOLS(nes)
CORE_OVL_SYMBOLS(gbc)
CORE_OVL_SYMBOLS(sms)
CORE_OVL_SYMBOLS(msx)
CORE_OVL_SYMBOLS(genesis)
CORE_OVL_SYMBOLS(doom)
CORE_OVL_SYMBOLS(darkforces)
CORE_OVL_SYMBOLS(pico8)

extern const uint8_t core_ovl_store[];

typedef struct {
  uint32_t offset, size;
} store_entry_t;

typedef struct {
  uint32_t magic, count;
  store_entry_t entry[];
} store_header_t;

typedef struct {
  const char *name;
  size_t size;
  const ctor_fn *ctors, *ctors_end;
} member_t;

#define CORE_OVL_MEMBER(n)                                                                                            \
  {#n, (size_t)(uintptr_t)_core_ovl_##n##_size, _core_ovl_ctors_##n##_start, _core_ovl_ctors_##n##_end}

// the store header's order (tools/core_overlay.py CORES)
static const member_t s_members[] = {
    CORE_OVL_MEMBER(nes),     CORE_OVL_MEMBER(gbc),  CORE_OVL_MEMBER(sms),        CORE_OVL_MEMBER(msx),
    CORE_OVL_MEMBER(genesis), CORE_OVL_MEMBER(doom), CORE_OVL_MEMBER(darkforces), CORE_OVL_MEMBER(pico8),
};
#define NUM_MEMBERS (int)(sizeof(s_members) / sizeof(s_members[0]))

static int s_loaded = -1;
static uint8_t *s_data;  // the PSRAM block (data-bus view)
static void *s_code;     // its executable mapping at the overlay address
static size_t s_mapped;  // bytes mapped
static uint32_t s_constructed; // bit per member: its constructors have run

bool core_overlay_load(const char *core) {
  int i = 0;
  while (i < NUM_MEMBERS && strcmp(core, s_members[i].name) != 0) {
    i++;
  }
  if (i == NUM_MEMBERS) {
    ESP_LOGE(TAG, "no core '%s' in the overlay", core);
    return false;
  }
  if (i == s_loaded) {
    return true;
  }
  core_overlay_unload();
  const member_t *m = &s_members[i];
  const store_header_t *h = (const store_header_t *)core_ovl_store;
  if (h->magic != MAGIC || (int)h->count != NUM_MEMBERS || h->entry[i].size != m->size) {
    ESP_LOGE(TAG, "%s: the flash store does not hold its code (was tools/core_overlay.py image run?)", core);
    return false;
  }
  if (m->size > (size_t)(_core_ovl_end - _core_ovl_start)) {
    ESP_LOGE(TAG, "%s: %u bytes of code, more than the overlay's range", core, (unsigned)m->size);
    return false;
  }
  if (m->size == 0) {
    s_loaded = i;
    return true;
  }
  const int64_t t0 = esp_timer_get_time();
  const size_t mapped = (m->size + PAGE - 1) & ~(PAGE - 1);
  uint8_t *data = heap_caps_aligned_alloc(PAGE, mapped, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!data) {
    ESP_LOGE(TAG, "%s: no %u KB of PSRAM for its code (largest free block %u KB)", core, (unsigned)(mapped / 1024),
             (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
    return false;
  }
  // the heap's PSRAM is one linear mapping: the block's pages are consecutive
  esp_paddr_t paddr = 0, page_paddr = 0;
  mmu_target_t target, page_target;
  bool linear = esp_mmu_vaddr_to_paddr(data, &paddr, &target) == ESP_OK && target == MMU_TARGET_PSRAM0;
  for (size_t off = PAGE; linear && off < mapped; off += PAGE) {
    linear = esp_mmu_vaddr_to_paddr(data + off, &page_paddr, &page_target) == ESP_OK &&
             page_target == MMU_TARGET_PSRAM0 && page_paddr == paddr + off;
  }
  void *code = NULL;
  esp_err_t err = linear ? esp_mmu_map_virt((esp_vaddr_t)(uintptr_t)_core_ovl_start, paddr, mapped, MMU_TARGET_PSRAM0,
                                            MMU_MEM_CAP_EXEC | MMU_MEM_CAP_32BIT, ESP_MMU_MMAP_FLAG_PADDR_SHARED, &code)
                         : ESP_ERR_INVALID_STATE;
  if (err != ESP_OK || code != (void *)_core_ovl_start) {
    ESP_LOGE(TAG, "%s: cannot map its PSRAM block at %p: %s", core, (void *)_core_ovl_start, esp_err_to_name(err));
    if (err == ESP_OK) {
      esp_mmu_unmap(code);
    }
    heap_caps_free(data);
    return false;
  }
  memcpy(data, core_ovl_store + h->entry[i].offset, m->size);
  // written through the data cache: write it back to PSRAM; then drop the
  // overlay range from the instruction cache (it may hold another core's code)
  esp_cache_msync(data, mapped, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
  esp_cache_msync(code, mapped, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_TYPE_INST);
  __asm__ volatile("isync" ::: "memory");
  s_data = data;
  s_code = code;
  s_mapped = mapped;
  s_loaded = i;
  const int64_t t1 = esp_timer_get_time();
  unsigned ctors = 0;
  if (!(s_constructed & (1u << i))) {
    // the core's static constructors, once per boot (its data persists); the
    // table is part of the code, read through the data-bus view
    s_constructed |= 1u << i;
    const uint8_t *table = data + ((const uint8_t *)m->ctors - _core_ovl_start);
    ctors = (unsigned)(m->ctors_end - m->ctors);
    for (unsigned k = 0; k < ctors; k++) {
      ctor_fn fn;
      memcpy(&fn, table + k * sizeof(fn), sizeof(fn));
      fn();
    }
  }
  ESP_LOGI(TAG, "%s: %u KB of code loaded in %lld ms (%u KB of PSRAM, %u constructors)", core,
           (unsigned)(m->size / 1024), (t1 - t0) / 1000, (unsigned)(mapped / 1024), ctors);
  return true;
}

void core_overlay_unload(void) {
  if (s_code) {
    // drop the code from the instruction cache too, so that anything of the
    // core that still runs faults instead of running it
    esp_cache_msync(s_code, s_mapped, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_TYPE_INST);
    esp_mmu_unmap(s_code);
    heap_caps_free(s_data);
    ESP_LOGI(TAG, "%s: unloaded (%u KB of PSRAM freed)", s_members[s_loaded].name, (unsigned)(s_mapped / 1024));
  }
  s_code = NULL;
  s_data = NULL;
  s_mapped = 0;
  s_loaded = -1;
}

const char *core_overlay_loaded(void) { return s_loaded >= 0 ? s_members[s_loaded].name : ""; }
