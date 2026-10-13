#pragma once

#include <cstdint>
#include <cstddef>

#include "shared_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Internal RAM the Genesis must leave for the rest of the system. The core's
// hot buffers prefer internal RAM in priority order (work RAM, YM2612 tables,
// CPU/chip state, Z80 RAM, line buffers, audio) and spill to PSRAM once an
// allocation would push free internal RAM below this. Measured: with no rule
// the Genesis left 36 bytes in the main internal heap at launch, and drivers
// that then could not get a few hundred bytes in the first frame hung the box.
#define GENESIS_INTERNAL_RESERVE_BYTES (16 * 1024)

// True if allocating `size` bytes of internal RAM would leave at least
// GENESIS_INTERNAL_RESERVE_BYTES free.
bool genesis_internal_has_headroom(size_t size);

// Initialize Genesis shared memory and hardware components (everything except
// VRAM, in internal-RAM priority order)
void genesis_init_shared_memory(void);

// Allocate VRAM. Called last, after the caller has placed its own hot buffers:
// allocation order is internal-RAM priority, and VRAM is the buffer to spill.
void genesis_init_shared_vram(void);

// Free Genesis shared memory
void genesis_free_shared_memory(void);

// Get pointers to shared memory regions
void genesis_get_memory_regions(uint8_t** vram, uint8_t** m68k_ram, uint8_t** z80_ram, 
                              int16_t** lfo_pm_table, int** tl_tab, int16_t** audio_buffer,
                              uint16_t** cram, uint16_t** vsram, uint8_t** sat_cache,
                              uint8_t** fifo, uint8_t** vdp_regs);

#ifdef __cplusplus
}
#endif
