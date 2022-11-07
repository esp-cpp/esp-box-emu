#pragma once

#include <string>

void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size);
void run_gameboy_rom();
void deinit_gameboy();
