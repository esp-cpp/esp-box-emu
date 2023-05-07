#pragma once

#include <string>
#include <string_view>
#include <vector>

void set_gb_video_original();
void set_gb_video_fit();
void set_gb_video_fill();
void reset_gameboy();
void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size);
void load_gameboy(std::string_view save_path);
void save_gameboy(std::string_view save_path);
void start_gameboy_tasks();
void stop_gameboy_tasks();
void run_gameboy_rom();
void deinit_gameboy();
std::vector<uint8_t> get_gameboy_video_buffer();
