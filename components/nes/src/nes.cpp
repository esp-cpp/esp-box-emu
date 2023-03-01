#include "nes.hpp"

#ifdef USE_NES_NOFRENDO
extern "C" {
#include "event.h"
#include <nes.h>
#include <nesstate.h>
}

static nes_t* console_nes;
#endif

#include <string>

#include "fs_init.hpp"
#include "format.hpp"
#include "spi_lcd.h"
#include "st7789.hpp"

static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = true;

void set_nes_video_original() {
  scaled = false;
  filled = false;
  osd_set_video_scale(false);
}

void set_nes_video_fit() {
  scaled = true;
  filled = false;
  osd_set_video_scale(false);
}

void set_nes_video_fill() {
  scaled = false;
  filled = true;
  osd_set_video_scale(true);
}

static uint8_t first_frame = 0;
void init_nes(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
#ifdef USE_NES_NOFRENDO
  static bool initialized = false;
  if (!initialized) {
    event_init();
    osd_init();
    // gui_init();
    vidinfo_t video;
    osd_getvideoinfo(&video);
    vid_init(video.default_width, video.default_height, video.driver);
    console_nes = nes_create();
    event_set_system(system_nes);
  } else {
    nes_reset(HARD_RESET);
  }
  initialized = true;
  nes_insertcart(rom_filename.c_str(), console_nes);
  vid_setmode(NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
  nes_prep_emulation(nullptr, console_nes);
  first_frame = 1;
#endif
}

void run_nes_rom() {
#ifdef USE_NES_NOFRENDO
  auto start = std::chrono::high_resolution_clock::now();
  nes_emulateframe(first_frame);
  first_frame = 0;
  // frame rate should be 60 FPS, so 1/60th second is what we want to sleep for
  auto delay = std::chrono::duration<float>(1.0f/60.0f);
  std::this_thread::sleep_until(start + delay);
#endif
}

void load_nes(std::string_view save_path) {
  nes_prep_emulation((char *)save_path.data(), console_nes);
}

void save_nes(std::string_view save_path) {
  save_sram((char *)save_path.data(), console_nes);
}

void deinit_nes() {
#ifdef USE_NES_NOFRENDO
  nes_poweroff();
  // nes_destroy(&console_nes);
#endif
}
