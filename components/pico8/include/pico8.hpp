#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// PICO-8 (femto8) for esp-box-emu. The cart runs in its own FreeRTOS task;
// run_pico8_rom() only paces the emulator loop. `rom_filename` is a .p8 or
// .p8.png cart.
void init_pico8(const std::string& rom_filename, uint8_t* romdata, size_t rom_data_size);
void reset_pico8();
void run_pico8_rom();
void deinit_pico8();

void pause_pico8_tasks();
void resume_pico8_tasks();

// true once the cart has ended (PICO-8's own exit, a Lua error, or a load
// failure); the cart then returns to the emulator menu
bool pico8_quit_requested();

// Save states are not supported; carts keep their own cartdata() files.
void load_pico8(std::string_view save_path, int save_slot);
void save_pico8(std::string_view save_path, int save_slot);

std::span<uint8_t> get_pico8_video_buffer();
