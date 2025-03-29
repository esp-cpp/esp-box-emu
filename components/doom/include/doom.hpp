#pragma once

#include <string>
#include <string_view>
#include <vector>

void reset_doom();
void init_doom(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size);
void load_doom(std::string_view save_path);
void save_doom(std::string_view save_path);
void start_doom_tasks();
void stop_doom_tasks();
void run_doom_rom();
void deinit_doom();
void set_doom_video_original();
void set_doom_video_fit();
void set_doom_video_fill();
std::vector<uint8_t> get_doom_video_buffer();

extern "C" {
void doom_pause_video_task();
void doom_resume_video_task();
void doom_pause_audio_task();
void doom_resume_audio_task();
}
