#include "nes.hpp"

#ifdef USE_NES_NOFRENDO
extern "C" {
#include "event.h"
#include "gui.h"
#include <nes.h>
}

static nes_t* console_nes;
#endif

#include <string>

#include "format.hpp"
#include "spi_lcd.h"
#include "input.h"
#include "st7789.hpp"

static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = true;

void set_nes_video_original() {
  scaled = false;
  filled = false;
}

void set_nes_video_fit() {
  scaled = true;
  filled = false;
}

void set_nes_video_fill() {
  scaled = false;
  filled = true;
}

void init_nes(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  espp::St7789::set_offset((320-NES_SCREEN_WIDTH) / 2, 0);

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

  nes_insertcart(rom_filename.c_str(), console_nes);
  vid_setmode(NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
  nes_prep_emulation();
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
#ifdef USE_NES_NOFRENDO
  nes_poweroff();
  // nes_destroy(&console_nes);
#endif
}
