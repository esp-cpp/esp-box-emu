#pragma once

#include <string>
#include <string_view>
#include <vector>

void reset_doom();
void init_doom(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size);
void load_doom(std::string_view save_path);
void save_doom(std::string_view save_path);
void run_doom_rom();
void deinit_doom();
std::vector<uint8_t> get_doom_video_buffer();
