#include "sdkconfig.h"

#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/queue.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include "hal_i2c.hpp"
#include "input.h"
#include "i2s_audio.h"
#include "spi_lcd.h"
#include "format.hpp"
#include "st7789.hpp"
#include "task_monitor.hpp"

#include "drv2605.hpp"

#include "gbc_cart.hpp"
#include "nes_cart.hpp"
#include "sms_cart.hpp"
#include "heap_utils.hpp"
#include "string_utils.hpp"
#include "fs_init.hpp"
#include "gui.hpp"
#include "mmap.hpp"
#include "rom_info.hpp"

// from spi_lcd.cpp
extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

static bool operator==(const InputState& lhs, const InputState& rhs) {
  return
    lhs.a == rhs.a &&
    lhs.b == rhs.b &&
    lhs.x == rhs.x &&
    lhs.y == rhs.y &&
    lhs.select == rhs.select &&
    lhs.start == rhs.start &&
    lhs.up == rhs.up &&
    lhs.down == rhs.down &&
    lhs.left == rhs.left &&
    lhs.right == rhs.right &&
    lhs.joystick_select == rhs.joystick_select;
}

std::unique_ptr<Cart> make_cart(const RomInfo& info) {
  switch (info.platform) {
  case Emulator::GAMEBOY:
  case Emulator::GAMEBOY_COLOR:
    return std::make_unique<GbcCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
    break;
  case Emulator::NES:
    return std::make_unique<NesCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  case Emulator::SEGA_MASTER_SYSTEM:
  case Emulator::SEGA_GAME_GEAR:
    return std::make_unique<SmsCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  default:
    return nullptr;
  }
}

extern "C" void app_main(void) {
  fmt::print("Starting esp-box-emu...\n");

  // init nvs and partition
  init_memory();
  // init filesystem
  fs_init();
  // init the display subsystem
  lcd_init();
  // initialize the i2c buses for touchpad, imu, audio codecs, gamepad, haptics, etc.
  i2c_init();
  // init the audio subsystem
  audio_init();
  // init the input subsystem
  init_input();

  std::error_code ec;

  espp::Drv2605 haptic_motor(espp::Drv2605::Config{
      .device_address = espp::Drv2605::DEFAULT_ADDRESS,
      .write = std::bind(&espp::I2c::write, external_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      .read = std::bind(&espp::I2c::read_at_register, external_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
      .motor_type = espp::Drv2605::MotorType::LRA
    });
  // we're using an LRA motor, so select th LRA library.
  haptic_motor.select_library(6, ec);

  auto play_haptic = [&haptic_motor]() {
    std::error_code ec;
    haptic_motor.start(ec);
  };
  auto set_waveform = [&haptic_motor](int waveform) {
    std::error_code ec;
    haptic_motor.set_waveform(0, espp::Drv2605::Waveform::SOFT_BUMP, ec);
    haptic_motor.set_waveform(1, espp::Drv2605::Waveform::SOFT_FUZZ, ec);
    haptic_motor.set_waveform(2, (espp::Drv2605::Waveform)(waveform), ec);
    haptic_motor.set_waveform(3, espp::Drv2605::Waveform::END, ec);
  };

  fmt::print("initializing gui...\n");
  // initialize the gui
  Gui gui({
      .play_haptic = play_haptic,
      .set_waveform = set_waveform,
      .display = display,
      .log_level = espp::Logger::Verbosity::WARN
    });

  // the prefix for the filesystem (either littlefs or sdcard)
  std::string fs_prefix = MOUNT_POINT;

  // load the metadata.csv file, parse it, and add roms from it
  auto roms = parse_metadata(fs_prefix + "/metadata.csv");
  std::string boxart_prefix = fs_prefix + "/";
  for (auto& rom : roms) {
    gui.add_rom(rom.name, boxart_prefix + rom.boxart_path);
  }

  while (true) {
    // reset gui ready to play and user_quit
    gui.ready_to_play(false);
    while (!gui.ready_to_play()) {
      std::this_thread::sleep_for(50ms);
    }

    // have broken out of the loop, let the user know we're processing...
    haptic_motor.start(ec);

    // Now pause the LVGL gui
    display->pause();
    gui.pause();

    // ensure the display has been paused
    std::this_thread::sleep_for(100ms);

    auto selected_rom_index = gui.get_selected_rom_index();
    if (selected_rom_index < roms.size()) {
      fmt::print("Selected rom:\n");
      fmt::print("  index: {}\n", selected_rom_index);
      auto selected_rom_info = roms[selected_rom_index];
      fmt::print("  name:  {}\n", selected_rom_info.name);
      fmt::print("  path:  {}\n", selected_rom_info.rom_path);

      // Cart handles platform specific code, state management, etc.
      {
        fmt::print("Before emulation, minimum free heap: {}\n", heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
        fmt::print("Before emulation, free (default):    {}\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
        fmt::print("Before emulation, free (8-bit):      {}\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        fmt::print("Before emulation, free (DMA):        {}\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
        fmt::print("Before emulation, free (8-bit|DMA):  {}\n", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA));

        std::unique_ptr<Cart> cart(make_cart(selected_rom_info));

        // heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
        // heap_caps_print_heap_info(MALLOC_CAP_8BIT);
        // heap_caps_print_heap_info(MALLOC_CAP_DMA);
        // heap_caps_print_heap_info(MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
        // heap_caps_check_integrity_all(true);
        // heap_caps_dump_all();

        if (cart) {
          fmt::print("Running cart...\n");
          while (cart->run());
        } else {
          fmt::print("Failed to create cart!\n");
        }
      }
    } else {
      fmt::print("Invalid rom selected!\n");
    }

    std::this_thread::sleep_for(100ms);

    fmt::print("Resuming your regularly scheduled programming...\n");

    fmt::print("During emulation, minimum free heap: {}\n", heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));

    // need to reset to control the whole screen
    espp::St7789::clear(0,0,320,240);

    gui.resume();
    display->force_refresh();
    display->resume();
  }
}
