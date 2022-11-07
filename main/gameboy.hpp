#pragma once

#include <memory>
#include "format.hpp"
#include "spi_lcd.h"
#include "i2s_audio.h"
#include "input.h"
#include "st7789.hpp"

static const size_t gameboy_screen_width = 160;

void vblank_callback() {
  // fmt::print("VBLANK\n");
}

static const int audio_frame_size = 2*256;
static int16_t audio_frame[audio_frame_size];
static int audio_frame_index = 0;
void play_audio_sample(int16_t l, int16_t r) {
  // fmt::print("L: {} R: {}\n", l, r);
  audio_frame[audio_frame_index++] = l;
  audio_frame[audio_frame_index++] = r;
  if (audio_frame_index == audio_frame_size) {
    // audio_play_frame((uint8_t*)audio_frame, audio_frame_size);
    audio_frame_index = 0;
  }
}

#ifdef USE_GAMEBOY_GAMEBOYCORE
#include "gameboycore/gameboycore.h"

static std::shared_ptr<gb::GameboyCore> core;
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
#endif
#ifdef USE_GAMEBOY_GNUBOY
extern "C" {
#include "gnuboy.h"
#include "loader.h"
}
#endif
#ifdef USE_GAMEBOY_GAMEBOY
extern "C" {
namespace gbc{
#include "gameboy/timer.h"
#include "gameboy/rom.h"
#include "gameboy/mem.h"
#include "gameboy/cpu.h"
#include "gameboy/lcd.h"
  }
}
#endif
#ifdef USE_GAMEBOY_LIBGBC
#include "machine.hpp"
std::shared_ptr<gbc::Machine> gbc_machine;
#endif

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
#ifdef USE_GAMEBOY_GAMEBOY
  // gbc::rom_load(rom_filename.c_str());
  gbc::rom_init(romdata, rom_data_size);
  gbc::gb_lcd_init();
  gbc::mem_init();
  gbc::cpu_init();
#endif
#ifdef USE_GAMEBOY_LIBGBC
  gbc_machine = std::make_shared<gbc::Machine>(romdata, rom_data_size);

  // trap on V-blank interrupts
  gbc_machine->set_handler(gbc::Machine::VBLANK,
                           [] (gbc::Machine& machine, gbc::interrupt_t&)
                           {
                             const auto& pixels = machine.gpu.pixels();
                             const int num_lines = 24;
                             const int frame_offset = num_lines * gbc::GPU::SCREEN_W;
                             for (int l=0; l<gbc::GPU::SCREEN_H; l += num_lines) {
                               lcd_write_frame(0, l, gbc::GPU::SCREEN_W, num_lines,
                                               (const uint8_t*)&pixels[l * gbc::GPU::SCREEN_W]);
                             }
                           });
#endif
}

void run_gameboy_rom() {
  uint8_t _num_touches, _btn_state;
  uint16_t _x,_y;
  touchpad_read(&_num_touches, &_x, &_y, &_btn_state);
  static size_t frame = 0;
  static auto start = std::chrono::high_resolution_clock::now();
  frame++;
  if ((frame % 60) == 0) {
    auto end = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float>(end - start).count();
    fmt::print("gameboy: FPS {}\n", (float) frame / elapsed);
  }

#ifdef USE_GAMEBOY_GAMEBOYCORE
  core->emulateFrame();
#endif
#ifdef USE_GAMEBOY_GNUBOY
  emu_run();
#endif
#ifdef USE_GAMEBOY_GAMEBOY
  static int r = 0;
  int now;
  if(!gbc::cpu_cycle())
    return;
  now = gbc::cpu_get_cycles();
  while(now != r) {
    for(int i = 0; i < 4; i++) {
      if(!gbc::lcd_cycle())
        return;
    }
    r++;
  }
  gbc::timer_cycle();
  r = now;
#endif
#ifdef USE_GAMEBOY_LIBGBC
  gbc_machine->simulate_one_frame();
#endif
}

void deinit_gameboy() {
  fmt::print("quitting gameboy emulation!\n");
#ifdef USE_GAMEBOY_GAMEBOYCORE
  core.reset();
#endif
#ifdef USE_GAMEBOY_GNUBOY
#endif
#ifdef USE_GAMEBOY_LIBGBC
  gbc_machine.reset();
#endif
}
