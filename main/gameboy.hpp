#pragma once

#include <memory>
#include "format.hpp"
#include "spi_lcd.h"
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

void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  // WIDTH = 160, so 320-WIDTH is 160
  espp::St7789::set_offset((320-160) / 2, 0);

#ifdef USE_GAMEBOY_GAMEBOYCORE
  fmt::print("GAMEBOY enabled: GAMEBOYCORE\n");

  // Create an instance of the gameboy emulator core
  core = std::make_shared<gb::GameboyCore>();

  // Set callbacks for video and audio
  core->setScanlineCallback([](const gb::GPU::Scanline& scanline, int line){
    fmt::print("Line {}\n", line);
    // scanline is just std::array<Pixel, 160>, where pixel is uint8_t r,g,b
    // make array of lv_color_t
    static std::array<uint16_t, 160> color_data;
    size_t index = 0;
    for (auto &pixel : scanline) {
      color_data[index++] = make_color(pixel.r, pixel.g, pixel.b);
    }
    lcd_write_frame(0, line, 160, 1, (const uint8_t*)&color_data[0]);
  });
  core->setAudioSampleCallback([](int16_t l, int16_t r){
    fmt::print("L: {} R: {}\n", l, r);
  });

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
  fmt::print("gameboycore: emulating frame\n");
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
