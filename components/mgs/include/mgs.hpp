#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// Metal Gear Solid (The FoxdieTeam decompilation, built natively through psyz)
// for esp-box-emu. The game runs in its own FreeRTOS tasks: run_mgs_rom() only
// feeds it the gamepad and paces the emulator loop.
//
// `rom_filename` is the path of STAGE.DIR; every other game file (RADIO.DAT,
// FACE.DAT, ...) is expected in the same directory.
void init_mgs(const std::string& rom_filename, uint8_t* romdata, size_t rom_data_size);
void reset_mgs();
void run_mgs_rom();
void deinit_mgs();

void pause_mgs_tasks();
void resume_mgs_tasks();

// true once the game has ended (or failed to start); the cart then returns to
// the emulator menu
bool mgs_quit_requested();

// Save states are not supported (the port has no memory card yet).
void load_mgs(std::string_view save_path, int save_slot);
void save_mgs(std::string_view save_path, int save_slot);

std::span<uint8_t> get_mgs_video_buffer();
