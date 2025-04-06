#pragma once

#include <cstdint>
#include <cstddef>

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