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

void genesis_init_shared_memory(void) {
    // allocate m68k cpu state in shared memory
    m68k = (m68ki_cpu_core*)shared_malloc(sizeof(m68ki_cpu_core));

    VRAM = (uint8_t*)shared_malloc(VRAM_MAX_SIZE); // 0x10000 (64kB) for VRAM
    ZRAM = (uint8_t*)shared_malloc(MAX_Z80_RAM_SIZE); // 0x2000 (8kB) for Z80 RAM

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

    tl_tab = (signed int*)shared_malloc(13*2*256 * sizeof(signed int));
}

void genesis_free_shared_memory(void) {
    // Clear all shared memory
    shared_mem_clear();
}
