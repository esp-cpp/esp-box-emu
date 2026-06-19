// Dual-core support for the Genesis emulator.
//
// Core 0 runs the 68000 + VDP; core 1 runs the sound unit (Z80 + YM2612 +
// SN76489). The 68000 occasionally writes the YM2612 / SN76489 registers and
// reads the YM2612 status. Those chips are owned by core 1, so instead of
// touching their state directly the 68000 (core 0) hands writes to a lock-free
// single-producer/single-consumer queue that the core-1 sound task drains in
// FIFO order, and reads a published snapshot of the YM2612 status.
//
// GENESIS_DUAL_CORE is the single source of truth for the feature flag and is
// shared by the C emulator sources (bus, vdp_mem) and the C++ driver.
#pragma once

#include <stdint.h>

// 0 = single-core (known-good path; zero overhead). 1 = dual-core.
#define GENESIS_DUAL_CORE 1

#ifdef __cplusplus
extern "C" {
#endif

enum {
  GEN_SND_YM2612 = 0,
  GEN_SND_SN76489 = 1,
};

// --- Producer side: called from the 68000 (core 0) -------------------------
// Enqueue a sound-chip register write (applied later, in order, by core 1).
// In single-core builds these are never referenced (the bus calls the chip
// write functions directly), so they cost nothing.
void genesis_sound_queue_push(uint8_t kind, uint8_t addr, uint8_t value, int cycles);

// Relaxed snapshot of the YM2612 status byte (busy/timer flags) published by
// core 1; read by the 68000 in place of YM2612Read().
unsigned int genesis_ym2612_status_peek(void);

// --- Consumer side: called from the core-1 sound task ----------------------
// Apply all queued writes via the real chip write functions (FIFO order).
void genesis_sound_queue_drain(void);
// Publish the current YM2612 status byte for the 68000 to read.
void genesis_ym2612_status_publish(unsigned int status);

// --- Lifecycle -------------------------------------------------------------
void genesis_sound_queue_reset(void);   // clear the queue (frame start / init)
uint32_t genesis_sound_queue_dropped(void); // count of writes dropped on overflow

#ifdef __cplusplus
}
#endif
