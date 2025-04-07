#pragma once

#include <cstdint>
#include <cstddef>

#include "shared_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Genesis shared memory and hardware components
void genesis_init_shared_memory(void);

// Free Genesis shared memory
void genesis_free_shared_memory(void);

// Get pointers to shared memory regions
void genesis_get_memory_regions(uint8_t** vram, uint8_t** m68k_ram, uint8_t** z80_ram, 
                              int32_t** lfo_pm_table, int** tl_tab, int16_t** audio_buffer,
                              uint16_t** cram, uint16_t** vsram, uint8_t** sat_cache,
                              uint8_t** fifo, uint8_t** vdp_regs);

#ifdef __cplusplus
}
#endif
