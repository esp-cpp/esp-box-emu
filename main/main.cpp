#include "nes.hpp"
#include "gameboy.hpp"

#include "sdkconfig.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include "spi_lcd.h"
#include "format.hpp"
#include "st7789.hpp"
#include "task_monitor.hpp"

#include "heap_utils.hpp"
#include "string_utils.hpp"
#include "fs_init.hpp"
#include "gui.hpp"
#include "mmap.hpp"
#include "lv_port_fs.h"
#include "rom_info.hpp"

extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

// NOTE: to see the indev configuration for the esp32 s3 box, look at
// bsp/src/boards/esp32_s3_box.c:56. Regarding whether it uses the FT5x06 or the
// TT21100, it uses the tp_prob function which checks to see if the devie
// addresses for those chips exist on the i2c bus (indev_tp.c:37)

extern "C" void app_main(void) {
  fmt::print("Starting esp-box-emu...\n");

  // init nvs and partition
  init_memory();
  // init filesystem
  fs_init();
  // init the display subsystem
  lcd_init();

  // initialize the gui
  Gui gui({
      .display = display
    });

  std::atomic<bool> quit{false};

  fmt::print("{}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();

  fmt::print("initializing the lv FS port...\n");

  lv_port_fs_init();

  // TODO: see line 230 of lvgl/src/extra/libs/sjpg/lv_sjpg.c
  //       The binary sjpg has a string "_SJPG__" as the first 8 bytes, followed by the raw

  // TODO: use the mmap functionality to copy the sjpg (binary) data into the
  // partition for each image and then build a lv_img_dsc_t struct based on it
  // to pass to set source, this will prevent LVGL from having to load it in
  // other ways?
  //
  // example for mario:
  //   lv_img_dsc_t boxart_mario = {
  //     .header.always_zero = 0,
  //     .header.w = 100,
  //     .header.h = 100,
  //     .data_size = 9210,    // 7428 for zelda
  //     .header.cf = LV_IMG_CF_RAW,
  //     .data = boxart_mario_map,
  //   };

  // load the metadata.csv file, parse it, and add roms from it
  auto roms = parse_metadata("/littlefs/metadata.csv");
  std::string boxart_prefix = "L:";
  for (auto& rom : roms) {
    gui.add_rom(rom.name, boxart_prefix + rom.boxart_path);
    // gui.next();
  }
  gui.next();
  /*
  while (true) {
    // scroll through the rom list forever :)
    gui.next();
    std::this_thread::sleep_for(5s);
  }
  */
  std::this_thread::sleep_for(2s);

  // Now pause the LVGL gui
  display->pause();
  gui.pause();

  fmt::print("{}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();

  // ensure the display has been paused
  std::this_thread::sleep_for(500ms);

  auto selected_rom_index = gui.get_selected_rom_index();
  auto selected_rom_info = roms[selected_rom_index];

  // copy the rom into the nesgame partition and memory map it
  std::string fs_prefix = "/littlefs/";
  std::string rom_filename = fs_prefix + selected_rom_info.rom_path;
  size_t rom_size_bytes = copy_romdata_to_nesgame_partition(rom_filename);
  while (!rom_size_bytes) {
    fmt::print("Could not copy {} into nesgame_partition!\n", rom_filename);
    std::this_thread::sleep_for(10s);
  }
  uint8_t* romdata = get_mmapped_romdata();

  fmt::print("Got mmapped romdata for {}, length={}\n", rom_filename, rom_size_bytes);

  // Clear the display
  espp::St7789::clear(0,0,320,240);
  int x_offset, y_offset;
  // store the offset for resetting to later (after emulation ends)
  espp::St7789::get_offset(x_offset, y_offset);

  switch (selected_rom_info.platform) {
  case Emulator::GAMEBOY:
  case Emulator::GAMEBOY_COLOR:
    init_gameboy(rom_filename, romdata, rom_size_bytes);
    while (!quit) {
      run_gameboy_rom();
    }
    break;
  case Emulator::NES:
    init_nes(rom_filename, display->vram0(), romdata, rom_size_bytes);
    while (!quit) {
      run_nes_rom();
    }
    break;
  default:
    break;
  }

  fmt::print("Resuming your regularly scheduled programming...\n");

  // If we got here, it's because the user sent a "quit" command
  espp::St7789::clear(0,0,320,240);
  // reset the offset
  espp::St7789::set_offset(x_offset, y_offset);
  display->resume();
  gui.resume();
  while (true) {
    std::this_thread::sleep_for(100ms);
  }
}
