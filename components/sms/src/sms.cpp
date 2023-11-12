#include "sms.hpp"

extern "C" {
#include <system.h>
}

#include <string>

#include "fs_init.hpp"
#include "format.hpp"
#include "spi_lcd.h"
#include "st7789.hpp"

static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = true;

static constexpr int SMS_SCREEN_WIDTH = 256;
static constexpr int SMS_SCREEN_HEIGHT = 192;

void set_sms_video_original() {
  scaled = false;
  filled = false;
}

void set_sms_video_fit() {
  scaled = true;
  filled = false;
}

void set_sms_video_fill() {
  scaled = false;
  filled = true;
}

void reset_sms() {
  system_reset();
}

void init_sms(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  static bool initialized = false;
  initialized = true;
  system_init2();
  system_poweron();
}

void run_sms_rom() {
  auto start = std::chrono::high_resolution_clock::now();
  bool skip_frame = false;
  system_frame(skip_frame);
  // frame rate should be 60 FPS, so 1/60th second is what we want to sleep for
  auto delay = std::chrono::duration<float>(1.0f/60.0f);
  std::this_thread::sleep_until(start + delay);
}

void load_sms(std::string_view save_path) {
}

void save_sms(std::string_view save_path) {
}

std::vector<uint8_t> get_sms_buffer() {
  std::vector<uint8_t> frame(SMS_SCREEN_WIDTH * SMS_SCREEN_HEIGHT * 2);
  uint8_t *frame_buffer0 = get_frame_buffer0();
  return frame;
}

void stop_sms_tasks() {
}

void start_sms_tasks() {
}

void deinit_sms() {
  system_poweroff();
}
