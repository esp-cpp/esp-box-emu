#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

void reset_doom();
void init_doom(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size);
void load_doom(int save_slot);
void save_doom(int save_slot);
void run_doom_rom();
void deinit_doom();
std::span<uint8_t> get_doom_video_buffer();
