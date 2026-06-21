#pragma once

#include <cstdint>
#include <cstddef>

// Game Boy (Color) audio sizing. The shared PCM buffer allocation (in
// gbc_shared_memory.cpp) and pcm.len (in gameboy.cpp) BOTH derive from these,
// so they cannot diverge: if pcm.len implies a larger buffer than is allocated,
// the gnuboy sound mixer overruns it and corrupts the heap.
#define GAMEBOY_AUDIO_SAMPLE_RATE 32768
// Size (in bytes) of the shared PCM buffer: ~1/5 second of stereo (2ch),
// 16-bit (2 byte) audio. pcm.len = GBC_AUDIO_BUFFER_SIZE / sizeof(int16_t).
#define GBC_AUDIO_BUFFER_SIZE (GAMEBOY_AUDIO_SAMPLE_RATE * 2 * 2 / 5)

#ifdef __cplusplus
extern "C" {
#endif

// Initialize GBC shared memory and hardware components
void gbc_init_shared_memory(void);

// Free GBC shared memory
void gbc_free_shared_memory(void);

// Get pointers to shared memory regions
void gbc_get_memory_regions(uint8_t** vram, uint8_t** wram, uint8_t** audio);

#ifdef __cplusplus
}
#endif 