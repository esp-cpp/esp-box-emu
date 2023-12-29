#include "sms.hpp"

// include private sms headers
extern "C" {
#include "sms.h"
#include "wrapemu.h"
#include "vdp.h"
}

#include <string>

#include "fs_init.hpp"
#include "format.hpp"
#include "spi_lcd.h"
#include "st7789.hpp"

static const int SMS_SCREEN_WIDTH = 256;
static const int SMS_SCREEN_HEIGHT = 192;
static const int SMS_VISIBLE_HEIGHT = 192;

static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = true;

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
}

void emu_DrawLinePal16(unsigned char * VBuf, int width, int height, int line) {
  // TODO: implement
}

void init_sms(uint8_t *romdata, size_t rom_data_size) {
  sms_Init();
  sms.ram = (uint8_t*)get_vram0();
  sms.sram= (uint8_t*)get_vram1();
  sms.dummy = get_frame_buffer1();
  vdp.vram = get_frame_buffer0();
  sms_Start(romdata, rom_data_size);
}

void init_gg(uint8_t *romdata, size_t rom_data_size) {
  sms_Init();
  sms.ram = (uint8_t*)get_vram0();
  sms.sram= (uint8_t*)get_vram1();
  sms.dummy = get_frame_buffer1();
  vdp.vram = get_frame_buffer0();
  gg_Start(romdata, rom_data_size);
}

void run_sms_rom() {
  auto start = std::chrono::high_resolution_clock::now();
  sms_Step();
  // frame rate should be 60 FPS, so 1/60th second is what we want to sleep for
  auto delay = std::chrono::duration<float>(1.0f/60.0f);
  std::this_thread::sleep_until(start + delay);
}

void load_sms(std::string_view save_path) {
  // sms_prep_emulation((char *)save_path.data(), console_sms);
}

void save_sms(std::string_view save_path) {
  // save_sram((char *)save_path.data(), console_sms);
}

std::vector<uint8_t> get_sms_video_buffer() {
  std::vector<uint8_t> frame(SMS_SCREEN_WIDTH * SMS_VISIBLE_HEIGHT * 2);
  // the frame data for the SMS is stored in frame_buffer0 as a 8 bit index into the palette
  // we need to convert this to a 16 bit RGB565 value
  // uint8_t *frame_buffer0 = get_frame_buffer0();
  // uint16_t *palette = nullptr; // get_sms_palette();
  // for (int i = 0; i < SMS_SCREEN_WIDTH * SMS_VISIBLE_HEIGHT; i++) {
  //   uint8_t index = frame_buffer0[i];
  //   uint16_t color = palette[index];
  //   frame[i * 2] = color & 0xFF;
  //   frame[i * 2 + 1] = color >> 8;
  // }
  return frame;
}

void stop_sms_tasks() {
}

void start_sms_tasks() {
}

void deinit_sms() {
  sms_DeInit();
}
