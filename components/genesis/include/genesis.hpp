#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "genesis_shared_memory.hpp"
#include "z80_shared_memory.hpp"
#include "box-emu.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the Genesis emulator
void init_genesis(uint8_t *romdata, size_t rom_data_size);

// Reset the Genesis emulator
void reset_genesis();

// Load a Genesis save state
void load_genesis(std::string_view save_path);

// Save a Genesis save state
void save_genesis(std::string_view save_path);

// Run the Genesis emulator
void run_genesis_rom();

// Deinitialize the Genesis emulator
void deinit_genesis();

// Get the Genesis video buffer
std::span<uint8_t> get_genesis_video_buffer();

#ifdef __cplusplus
}
#endif
