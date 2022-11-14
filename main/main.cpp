#include "nes.hpp"
#include "gameboy.hpp"

#include "sdkconfig.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include "input.h"
#include "i2s_audio.h"
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

// from spi_lcd.cpp
extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

// GB
#define GAMEBOY_WIDTH (160)
#define GAMEBOY_HEIGHT (144)
// SMS
#define SMS_WIDTH (256)
#define SMS_HEIGHT (192)
// GG
#define GAMEGEAR_WIDTH (160)
#define GAMEGEAR_HEIGHT (144)

extern "C" void app_main(void) {
  fmt::print("Starting esp-box-emu...\n");

  // init nvs and partition
  init_memory();
  // init filesystem
  fs_init();
  // init the display subsystem
  lcd_init();
  // init the audio subsystem
  audio_init();
  // init the input subsystem
  init_input();

  fmt::print("initializing gui...\n");
  // initialize the gui
  Gui gui({
      .display = display,
      .log_level = espp::Logger::Verbosity::WARN
    });

  fmt::print("initializing the lv FS port...\n");
  lv_port_fs_init();

  // load the metadata.csv file, parse it, and add roms from it
  auto roms = parse_metadata("/littlefs/metadata.csv");
  std::string boxart_prefix = "L:";
  for (auto& rom : roms) {
    gui.add_rom(rom.name, boxart_prefix + rom.boxart_path);
  }

  int x_offset, y_offset;
  // store the offset for resetting to later (after emulation ends)
  espp::St7789::get_offset(x_offset, y_offset);

  while (true) {
    // reset gui ready to play and user_quit
    gui.ready_to_play(false);
    reset_user_quit();

    while (!gui.ready_to_play()) {
      // TODO: would be better to make this an actual LVGL input device instead
      // of this..
      static struct InputState prev_state;
      static struct InputState curr_state;
      get_input_state(&curr_state);
      if (curr_state.up && !prev_state.up) {
        gui.previous();
      } else if (curr_state.down && !prev_state.down) {
        gui.next();
      } else if (curr_state.start) {
        // same as play button was pressed, just exit the loop!
        break;
      }
      prev_state = curr_state;
      std::this_thread::sleep_for(100ms);
    }

    // update the audio level according to the gui
    set_audio_volume(gui.get_audio_level());

    // Now pause the LVGL gui
    display->pause();
    gui.pause();

    // set the video scaling for the emulation
    auto video_scaling = gui.get_video_setting();
    switch (video_scaling) {
    case Gui::VideoSetting::ORIGINAL:
      set_nes_video_original();
      set_gb_video_original();
      break;
    case Gui::VideoSetting::FIT:
      set_nes_video_fit();
      set_gb_video_fit();
      break;
    case Gui::VideoSetting::FILL:
      set_nes_video_fill();
      set_gb_video_fill();
      break;
    }

    // ensure the display has been paused
    std::this_thread::sleep_for(500ms);

    auto selected_rom_index = gui.get_selected_rom_index();
    fmt::print("Selected rom index: {}\n", selected_rom_index);
    auto selected_rom_info = roms[selected_rom_index];

    // copy the rom into the nesgame partition and memory map it
    std::string fs_prefix = "/littlefs/";
    std::string rom_filename = fs_prefix + selected_rom_info.rom_path;
    size_t rom_size_bytes = copy_romdata_to_nesgame_partition(rom_filename);
    if (rom_size_bytes) {
      uint8_t* romdata = get_mmapped_romdata();
      fmt::print("Got mmapped romdata for {}, length={}\n", rom_filename, rom_size_bytes);

      // Clear the display
      espp::St7789::clear(0,0,320,240);

      switch (selected_rom_info.platform) {
      case Emulator::GAMEBOY:
      case Emulator::GAMEBOY_COLOR:
        init_gameboy(rom_filename, romdata, rom_size_bytes);
        while (!user_quit()) {
          run_gameboy_rom();
        }
        deinit_gameboy();
        break;
      case Emulator::NES:
        init_nes(rom_filename, romdata, rom_size_bytes);
        while (!user_quit()) {
          run_nes_rom();
        }
        deinit_nes();
        break;
      default:
        break;
      }
    } else {
      fmt::print("Could not copy {} into nesgame_partition!\n", rom_filename);
    }

    fmt::print("quitting emulation...\n");

    std::this_thread::sleep_for(500ms);

    fmt::print("Resuming your regularly scheduled programming...\n");
    // need to reset to control the whole screen
    espp::St7789::clear(0,0,320,240);
    // reset the offset
    espp::St7789::set_offset(x_offset, y_offset);

    display->force_refresh();

    display->resume();
    gui.resume();
  }
}
