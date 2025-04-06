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

static const char *TAG = "genesis_shared_memory";

extern unsigned short *CRAM; // [CRAM_MAX_SIZE];           // CRAM - Palettes
extern unsigned char *SAT_CACHE; // [SAT_CACHE_MAX_SIZE];  // Sprite cache
extern unsigned char *gwenesis_vdp_regs; // [REG_SIZE];    // Registers
extern unsigned short *fifo; // [FIFO_SIZE];               // Fifo
extern unsigned short *CRAM565; // [CRAM_MAX_SIZE * 4];    // CRAM - Palettes
extern unsigned short *VSRAM; // [VSRAM_MAX_SIZE];         // VSRAM - Scrolling

void genesis_init_shared_memory(void) {
    // allocate m68k cpu state in shared memory
    m68k = (m68ki_cpu_core*)shared_malloc(sizeof(m68ki_cpu_core));
    if (!m68k) {
        ESP_LOGE(TAG, "Failed to allocate M68K CPU state");
        return;
    }

    ym2612 = (YM2612*)shared_malloc(sizeof(YM2612));
    OPNREGS = (uint8_t*)shared_malloc(512);
    sin_tab = (unsigned int*)shared_malloc(SIN_LEN * sizeof(unsigned int));

    render_buffer = (uint8_t*)shared_malloc(SCREEN_WIDTH + PIX_OVERFLOW*2);
    sprite_buffer = (uint8_t*)shared_malloc(SCREEN_WIDTH + PIX_OVERFLOW*2);

    CRAM = (uint16_t*)shared_malloc(CRAM_MAX_SIZE * sizeof(uint16_t));
    SAT_CACHE = (uint8_t*)shared_malloc(SAT_CACHE_MAX_SIZE);
    gwenesis_vdp_regs = (uint8_t*)shared_malloc(REG_SIZE);
    fifo = (uint16_t*)shared_malloc(FIFO_SIZE * sizeof(uint16_t));
    CRAM565 = (uint16_t*)shared_malloc(CRAM_MAX_SIZE * 4 * sizeof(uint16_t));
    VSRAM = (uint16_t*)shared_malloc(VSRAM_MAX_SIZE * sizeof(uint16_t));
}

void genesis_free_shared_memory(void) {
    // Clear all shared memory
    shared_mem_clear();
}
