#include "nes.hpp"

#ifdef USE_NES_NOFRENDO
extern "C" {
#include "event.h"
#include "gui.h"
#include <nes.h>
#include <nesstate.h>
}

static nes_t* console_nes;
#endif

#include <string>

#include "fs_init.hpp"
#include "format.hpp"
#include "spi_lcd.h"
#include "input.h"
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

static std::string nes_savedir = "/sdcard/saves";
static std::string current_cart = "";

static std::string get_save_path(bool bypass_exist_check=false) {
  namespace fs = std::filesystem;
  fmt::print("creating: {}\n", nes_savedir);
  // fs::create_directories(nes_savedir);
  mkdirp(nes_savedir.c_str());
  auto save_path = nes_savedir + "/" + fs::path(current_cart).stem().string() + "_nes.sav";
  if (bypass_exist_check || fs::exists(save_path)) {
    fmt::print("found: {}\n", save_path);
    return save_path;
  } else {
    fmt::print("Could not find {}\n", save_path);
  }
  return "";
}

void init_nes(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
#ifdef USE_NES_NOFRENDO
  static bool initialized = false;
  if (!initialized) {
    event_init();
    osd_init();
    gui_init();
    vidinfo_t video;
    osd_getvideoinfo(&video);
    vid_init(video.default_width, video.default_height, video.driver);
    console_nes = nes_create();
    event_set_system(system_nes);
  } else {
    nes_reset(HARD_RESET);
  }
  initialized = true;
  current_cart = rom_filename;
  nes_insertcart(rom_filename.c_str(), console_nes);
  vid_setmode(NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
  nes_prep_emulation((char*)get_save_path().c_str(), console_nes);
#endif
}

void run_nes_rom() {
  // we have to call touchpad_read to determine if the user needs to quit...
  uint8_t _num_touches, _btn_state;
  uint16_t _x,_y;
  touchpad_read(&_num_touches, &_x, &_y, &_btn_state);
#ifdef USE_NES_NOFRENDO
  auto start = std::chrono::high_resolution_clock::now();
  nes_emulateframe(0);
  // frame rate should be 60 FPS, so 1/60th second is what we want to sleep for
  auto delay = std::chrono::duration<float>(1.0f/60.0f);
  std::this_thread::sleep_until(start + delay);
#endif
}

void deinit_nes() {
  // save state here
  save_sram((char*)get_save_path(true).c_str(), console_nes);
#ifdef USE_NES_NOFRENDO
  nes_poweroff();
  // nes_destroy(&console_nes);
#endif
}
