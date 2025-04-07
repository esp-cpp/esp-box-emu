#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "shared_memory.h"

void reset_sms();
void init_sms(uint8_t *romdata, size_t rom_data_size);
void init_gg(uint8_t *romdata, size_t rom_data_size);
void load_sms(std::string_view save_path);
void save_sms(std::string_view save_path);
void run_sms_rom();
void deinit_sms();
std::vector<uint8_t> get_sms_video_buffer();
