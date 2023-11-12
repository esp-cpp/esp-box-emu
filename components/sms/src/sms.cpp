#include "sms.hpp"

extern "C" {
#include <shared.h>
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

  // see sms.h for sms_t members such as paused/save/territory/console
	sms.use_fm = 0; // 0 = No YM2413 sound, 1 = YM2413 sound
	sms.territory = TERRITORY_EXPORT; // could also be TERRITORY_DOMESTIC

  // see system.h for bitmap_t members such as data/width/height/pitch/depth
	bitmap.data = get_frame_buffer0(); // appIramData->videodata;
	bitmap.width = 256;
	bitmap.height = 192;
	bitmap.pitch = 256;
  // bitmap.depth = 16;

  // see system.h for cart_t members such as rom/loaded/size/pages/etc.
	cart.rom = romdata;
  cart.size = rom_data_size;
	// cart.pages = ((512*1024)/0x4000);
  cart.pages = rom_data_size / 0x4000;

  system_init2();
  system_poweron();
}

void run_sms_rom() {
  auto start = std::chrono::high_resolution_clock::now();
  bool skip_frame = false;
  // TODO: set up the input structure
  // pass 0 to draw the current frame, 1 to omit the drawing process
  system_frame(skip_frame);
  // TODO: play sound using snd.buffer
  // TODO: render the frame using bitmap.data
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
  // copy the frame buffer to the vector
  std::copy(frame_buffer0, frame_buffer0 + frame.size(), frame.begin());
  return frame;
}

void stop_sms_tasks() {
}

void start_sms_tasks() {
}

void deinit_sms() {
  system_poweroff();
}
