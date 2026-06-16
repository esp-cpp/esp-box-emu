#include "genesis_shared_memory.hpp"
#include "shared_memory.h"
#include "esp_log.h"

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
    if (ptr != nullptr) {
        return ptr;
    }

    ESP_LOGW(TAG, "Allocating %s in PSRAM fallback", name);
    return allocate_shared(size, SHARED_MEM_PSRAM, region);
}

void genesis_init_shared_memory(void) {
    // allocate m68k cpu state in shared memory
    m68k = (m68ki_cpu_core*)allocate_shared(sizeof(m68ki_cpu_core), SHARED_MEM_INTERNAL);

    VRAM = (uint8_t*)allocate_shared(VRAM_MAX_SIZE, SHARED_MEM_INTERNAL, SHARED_MEM_CACHE_LINE); // 0x10000 (64kB) for VRAM
    ZRAM = (uint8_t*)allocate_shared_prefer_internal(MAX_Z80_RAM_SIZE, "ZRAM"); // 0x2000 (8kB) for Z80 RAM

    ym2612 = (YM2612*)allocate_shared(sizeof(YM2612), SHARED_MEM_INTERNAL);
    OPNREGS = (uint8_t*)allocate_shared(512, SHARED_MEM_INTERNAL);
    sin_tab = (unsigned int*)allocate_shared_prefer_internal(SIN_LEN * sizeof(unsigned int), "sin_tab");

    render_buffer = (uint8_t*)allocate_shared(SCREEN_WIDTH + PIX_OVERFLOW*2, SHARED_MEM_INTERNAL, SHARED_MEM_CACHE_LINE);
    sprite_buffer = (uint8_t*)allocate_shared(SCREEN_WIDTH + PIX_OVERFLOW*2, SHARED_MEM_INTERNAL, SHARED_MEM_CACHE_LINE);

    CRAM = (uint16_t*)allocate_shared(CRAM_MAX_SIZE * sizeof(uint16_t), SHARED_MEM_INTERNAL);
    SAT_CACHE = (uint8_t*)allocate_shared(SAT_CACHE_MAX_SIZE, SHARED_MEM_INTERNAL, SHARED_MEM_CACHE_LINE);
    gwenesis_vdp_regs = (uint8_t*)allocate_shared(REG_SIZE, SHARED_MEM_INTERNAL);
    fifo = (uint16_t*)allocate_shared(FIFO_SIZE * sizeof(uint16_t), SHARED_MEM_INTERNAL);
    CRAM565 = (uint16_t*)allocate_shared(CRAM_MAX_SIZE * 4 * sizeof(uint16_t), SHARED_MEM_INTERNAL);
    VSRAM = (uint16_t*)allocate_shared(VSRAM_MAX_SIZE * sizeof(uint16_t), SHARED_MEM_INTERNAL);

    tl_tab = (signed int*)allocate_shared_prefer_internal(13*2*256 * sizeof(signed int), "tl_tab");
}

void genesis_free_shared_memory(void) {
    // Clear all shared memory
    shared_mem_clear();
}
