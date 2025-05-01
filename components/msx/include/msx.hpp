#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

void reset_msx();
void init_msx(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size);
void load_msx(std::string_view save_path);
void save_msx(std::string_view save_path);
void start_msx_tasks();
void stop_msx_tasks();
void run_msx_rom();
void deinit_msx();
void set_msx_video_original();
void set_msx_video_fit();
void set_msx_video_fill();
std::span<uint8_t> get_msx_video_buffer();
