#pragma once

#include <string>

void init_nes(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size);
void run_nes_rom();
void deinit_nes();
void set_nes_video_original();
void set_nes_video_fit();
void set_nes_video_fill();
