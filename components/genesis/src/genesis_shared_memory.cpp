#include "genesis_shared_memory.hpp"
#include "shared_memory.h"
#include "esp_log.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "m68k.h"
#include "ym2612.h"
#include "gwenesis_bus.h"
#include "gwenesis_vdp.h"
}

extern unsigned char* VRAM;
extern unsigned short *CRAM; // [CRAM_MAX_SIZE];           // CRAM - Palettes
extern unsigned char *SAT_CACHE; // [SAT_CACHE_MAX_SIZE];  // Sprite cache
extern unsigned char *gwenesis_vdp_regs; // [REG_SIZE];    // Registers
extern unsigned short *fifo; // [FIFO_SIZE];               // Fifo
extern unsigned short *CRAM565; // [CRAM_MAX_SIZE * 4];    // CRAM - Palettes
extern unsigned short *VSRAM; // [VSRAM_MAX_SIZE];         // VSRAM - Scrolling

uint8_t *M68K_RAM = nullptr; // MAX_RAM_SIZE
uint8_t *ZRAM = nullptr; // MAX_Z80_RAM_SIZE
signed int *tl_tab = nullptr; // 13*2*TL_RES_LEN (13*2*256 * sizeof(signed int)) = 26624 bytes

static constexpr const char *TAG = "genesis_shared_memory";

static void *allocate_shared(size_t size, shared_mem_storage_t storage, shared_mem_region_t region = SHARED_MEM_DEFAULT) {
    shared_mem_request_t request = {
        .size = size,
        .region = region,
        .storage = storage,
    };
    return shared_mem_allocate(&request);
}

static void *allocate_shared_prefer_internal(size_t size, const char *name, shared_mem_region_t region = SHARED_MEM_DEFAULT) {
    void *ptr = allocate_shared(size, SHARED_MEM_INTERNAL, region);
    const bool internal = (ptr != nullptr);
    if (!ptr) {
        ptr = allocate_shared(size, SHARED_MEM_PSRAM, region);
    }
    // same format as allocate_hot_memory() in genesis.cpp: tools/perf_report.py
    // keys on it to show where each buffer landed
    printf("[genesis mem] %s: %u bytes -> %s\n", name, (unsigned)size, internal ? "INTERNAL" : "PSRAM");
    return ptr;
}

void genesis_init_shared_memory(void) {
    // Internal RAM priority (measured on the BOX-3 with the [genesis perf]
    // breakdown): M68K_RAM first (genesis.cpp init(), every 68000 instruction),
    // then the YM2612 lookup tables -- they are read on EVERY sample by the
    // core-1 sound unit, and when they fall to PSRAM their random reads thrash
    // the data cache the two cores share (sound frame +0.5 ms, 68k slower,
    // audible crackle). VRAM comes LAST: moving it internal measured no change
    // in the VDP render time, so it is the right buffer to spill to PSRAM.
    tl_tab = (signed int*)allocate_shared_prefer_internal(13*2*256 * sizeof(signed int), "tl_tab");
    sin_tab = (unsigned int*)allocate_shared_prefer_internal(SIN_LEN * sizeof(unsigned int), "sin_tab");

    // The small state structs and line buffers want internal RAM too, but a
    // tight internal budget (another core's leftovers, a bigger cache) must
    // degrade to PSRAM rather than fail init -- a failed init leaves the player
    // on a black screen.
    m68k = (m68ki_cpu_core*)allocate_shared_prefer_internal(sizeof(m68ki_cpu_core), "m68k");

    ZRAM = (uint8_t*)allocate_shared_prefer_internal(MAX_Z80_RAM_SIZE, "ZRAM"); // 0x2000 (8kB) for Z80 RAM

    ym2612 = (YM2612*)allocate_shared_prefer_internal(sizeof(YM2612), "ym2612");
    OPNREGS = (uint8_t*)allocate_shared_prefer_internal(512, "OPNREGS");

    render_buffer = (uint8_t*)allocate_shared_prefer_internal(SCREEN_WIDTH + PIX_OVERFLOW*2, "render_buffer", SHARED_MEM_CACHE_LINE);
    sprite_buffer = (uint8_t*)allocate_shared_prefer_internal(SCREEN_WIDTH + PIX_OVERFLOW*2, "sprite_buffer", SHARED_MEM_CACHE_LINE);

    CRAM = (uint16_t*)allocate_shared_prefer_internal(CRAM_MAX_SIZE * sizeof(uint16_t), "CRAM");
    SAT_CACHE = (uint8_t*)allocate_shared_prefer_internal(SAT_CACHE_MAX_SIZE, "SAT_CACHE", SHARED_MEM_CACHE_LINE);
    gwenesis_vdp_regs = (uint8_t*)allocate_shared_prefer_internal(REG_SIZE, "vdp_regs");
    fifo = (uint16_t*)allocate_shared_prefer_internal(FIFO_SIZE * sizeof(uint16_t), "fifo");
    CRAM565 = (uint16_t*)allocate_shared_prefer_internal(CRAM_MAX_SIZE * 4 * sizeof(uint16_t), "CRAM565");
    VSRAM = (uint16_t*)allocate_shared_prefer_internal(VSRAM_MAX_SIZE * sizeof(uint16_t), "VSRAM");

    // 64kB; internal only if the big block still has room (see above)
    VRAM = (uint8_t*)allocate_shared_prefer_internal(VRAM_MAX_SIZE, "VRAM", SHARED_MEM_CACHE_LINE);
}

void genesis_free_shared_memory(void) {
    // Clear all shared memory
    shared_mem_clear();
}
