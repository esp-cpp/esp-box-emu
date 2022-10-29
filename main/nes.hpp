#pragma once

#ifdef USE_NES_NES_LIB
#include "nes_lib/utils.h"
#include "nes_lib/Gamepak.h"
#include "nes_lib/InputDevice.h"
#include "nes_lib/cpu.h"
#include "nes_lib/PPU.h"
#include "nes_lib/PPUMemory.h"
#include "nes_lib/memory.h"
#include "nes_lib/nes_clock.h"

static std::shared_ptr<Gamepak> gamepak;
static std::shared_ptr<PPU> ppu;
static std::shared_ptr<InputDevice> joypad0;
static std::shared_ptr<InputDevice> joypad1;
static std::shared_ptr<NesCPUMemory> cpuMemory;
static std::shared_ptr<NesCpu> cpu;
#endif
#ifdef USE_NES_NOFRENDO
extern "C" {
#include "nofrendo.h"
}
#endif

#include <string>

#include "format.hpp"
#include "spi_lcd.h"
#include "st7789.hpp"

void init_nes(const std::string& rom_filename, uint16_t* vram_ptr, uint8_t *romdata, size_t rom_data_size) {
  // WIDTH = 256, so 320-WIDTH 64
  espp::St7789::set_offset((320-256) / 2, 0);

#ifdef USE_NES_NES_LIB
  fmt::print("NES enabled: NES_LIB\n");

  // now start the emulator
  gamepak = std::make_shared<Gamepak>(rom_filename);
  gamepak->initialize(romdata);
  ppu = std::make_shared<PPU>(gamepak.get(), vram_ptr);
  joypad0 = std::make_shared<InputDevice>(-1);
  joypad1 = std::make_shared<InputDevice>(-2);
  cpuMemory = std::make_shared<NesCPUMemory>(ppu.get(), gamepak.get(), joypad0.get(), joypad1.get());
  cpu = std::make_shared<NesCpu>(cpuMemory.get());
  ppu->assign_cpu(cpu.get());
  cpu->power_up();
  ppu->power_up();
#endif
#ifdef USE_NES_NOFRENDO
  fmt::print("NES enabled: NOFRENDO\n");
  char* args[1] = {(char *)rom_filename.c_str()};
  nofrendo_main(1, args);
#endif
}

void run_nes_rom() {
#ifdef USE_NES_NES_LIB
  static uint32_t cpu_counter = 7;
  static uint32_t ppu_counter = 0;

  bool generated_image = false;
  while (!generated_image) {
    cpu_counter += cpu->step().count();
    while (ppu_counter < cpu_counter*3) {
      generated_image = ppu->step();
      ppu_counter++;
    }
  }
#endif
#ifdef USE_NES_NOFRENDO
#endif
}

void deinit_nes() {
#ifdef USE_NES_NES_LIB
  cpu.reset();
  cpuMemory.reset();
  joypad1.reset();
  joypad0.reset();
  ppu.reset();
  gamepak.reset();
#endif
#ifdef USE_NES_NOFRENDO
#endif
}
