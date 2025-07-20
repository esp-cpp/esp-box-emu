#pragma once

#include <span>
#include <cstdint>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

// PICO-8 emulator initialization and control
void init_pico8(const char* rom_filename, const uint8_t* rom_data, size_t rom_size);
void deinit_pico8();
void reset_pico8();
void run_pico8_frame();

// Video functions
std::span<uint8_t> get_pico8_video_buffer();
int get_pico8_screen_width();
int get_pico8_screen_height();

// Input functions  
void set_pico8_input(uint8_t buttons);
void pico8_key_down(int key);
void pico8_key_up(int key);

// Audio functions
void get_pico8_audio_buffer(int16_t* buffer, size_t frames);
void set_pico8_audio_enabled(bool enabled);

// Save state functions
bool load_pico8_state(const char* path);
bool save_pico8_state(const char* path);

// Cartridge functions
bool load_pico8_cartridge(const uint8_t* data, size_t size);
const char* get_pico8_cartridge_title();

// PICO-8 button mapping
#define PICO8_BTN_LEFT   0x01
#define PICO8_BTN_RIGHT  0x02
#define PICO8_BTN_UP     0x04
#define PICO8_BTN_DOWN   0x08
#define PICO8_BTN_Z      0x10  // Primary action (A button)
#define PICO8_BTN_X      0x20  // Secondary action (B button)
#define PICO8_BTN_ENTER  0x40  // Start
#define PICO8_BTN_SHIFT  0x80  // Select

#ifdef __cplusplus
}
#endif