#pragma once

#include <string>
#include <string_view>
#include <vector>

void reset_sms();
void init_sms(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size);
void load_sms(std::string_view save_path);
void save_sms(std::string_view save_path);
void start_sms_tasks();
void stop_sms_tasks();
void run_sms_rom();
void deinit_sms();
void set_sms_video_original();
void set_sms_video_fit();
void set_sms_video_fill();
std::vector<uint8_t> get_sms_video_buffer();

void sms_pause_video_task();
void sms_resume_video_task();
void sms_pause_audio_task();
void sms_resume_audio_task();
