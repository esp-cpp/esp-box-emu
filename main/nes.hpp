#pragma once

extern "C" {
#include "event.h"
#include "gui.h"
#include <nes.h>
}

#include <string>

#include "format.hpp"
#include "spi_lcd.h"
#include "st7789.hpp"

nes_t* console_nes;

void init_nes(const std::string& rom_filename, uint16_t* vram_ptr, uint8_t *romdata, size_t rom_data_size) {
  espp::St7789::set_offset((320-NES_SCREEN_WIDTH) / 2, 0);

  event_init();

  osd_init();
  gui_init();

  vidinfo_t video;
  osd_getvideoinfo(&video);
  vid_init(video.default_width, video.default_height, video.driver);

  console_nes = nes_create();

  event_set_system(system_nes);

  nes_insertcart(rom_filename.c_str(), console_nes);

  vid_setmode(NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
}

void run_nes_rom() {
  nes_emulate();
}

void deinit_nes() {
  nes_poweroff();
  nes_destroy(&console_nes);
}
