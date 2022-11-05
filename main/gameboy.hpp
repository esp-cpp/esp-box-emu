#pragma once

#include <memory>
#include "format.hpp"
#include "spi_lcd.h"
#include "i2s_audio.h"
#include "st7789.hpp"

#ifdef USE_GAMEBOY_GAMEBOYCORE
#include "gameboycore/gameboycore.h"

static std::shared_ptr<gb::GameboyCore> core;
#endif
#ifdef USE_GAMEBOY_GNUBOY
extern "C" {
#include "gnuboy.h"
#include "loader.h"
}
#endif

static const size_t gameboy_screen_width = 160;

void render_scanline(const gb::GPU::Scanline& scanline, int line) {
  // fmt::print("Line {}\n", line);
  // scanline is just std::array<Pixel, gameboy_screen_width>, where pixel is uint8_t r,g,b
  // make array of lv_color_t
  static const size_t num_lines_to_flush = 48;
  static size_t num_lines = 0;
  static auto color_data = get_vram0();
  size_t index = num_lines * gameboy_screen_width;
  for (auto &pixel : scanline) {
    color_data[index++] = make_color(pixel.r, pixel.g, pixel.b);
  }
  num_lines++;
  if (num_lines == num_lines_to_flush) {
    lcd_write_frame(0, line - num_lines, gameboy_screen_width, num_lines, (const uint8_t*)&color_data[0]);
    num_lines = 0;
  }
}

void vblank_callback() {
  // fmt::print("VBLANK\n");
}

void play_audio_sample(int16_t l, int16_t r) {
  // fmt::print("L: {} R: {}\n", l, r);
}

void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  // WIDTH = gameboy_screen_width, so 320-WIDTH is gameboy_screen_width
  espp::St7789::set_offset((320-gameboy_screen_width) / 2, (240-144) / 2);

#ifdef USE_GAMEBOY_GAMEBOYCORE
  fmt::print("GAMEBOY enabled: GAMEBOYCORE, loading {} ({} bytes)\n", rom_filename, rom_data_size);

  // Create an instance of the gameboy emulator core
  core = std::make_shared<gb::GameboyCore>();
  // Set callbacks for video and audio
  core->setScanlineCallback(render_scanline);
  core->setVBlankCallback(vblank_callback);
  core->setAudioSampleCallback(play_audio_sample);
  // now load the rom
  fmt::print("Opening rom {}!\n", rom_filename);
  core->loadROM(romdata, rom_data_size);
#endif
#ifdef USE_GAMEBOY_GNUBOY
  fmt::print("GAMEBOY enabled: GNUBOY\n");
  loader_init_raw(romdata, rom_data_size);
  emu_reset();
#endif
}

void run_gameboy_rom() {
#ifdef USE_GAMEBOY_GAMEBOYCORE
  // static size_t frame = 0;
  // fmt::print("gameboycore: emulating frame {}\n", frame++);
  core->emulateFrame();
#endif
#ifdef USE_GAMEBOY_GNUBOY
  emu_run();
#endif
}

void deinit_gameboy() {
#ifdef USE_GAMEBOY_GAMEBOYCORE
  core.reset();
#endif
#ifdef USE_GAMEBOY_GNUBOY
#endif
}
