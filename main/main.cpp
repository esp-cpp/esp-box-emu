#include "sdkconfig.h"

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
#include "timer.hpp"
#include "usb.hpp"

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

[[maybe_unused]] static bool operator==(const InputState& lhs, const InputState& rhs) {
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
  haptic_motor.select_library(espp::Drv2605::Library::LRA, ec);

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

  auto battery_test_timer = espp::Timer({.name = "Timer 1",
      .period = 1s,
      .callback = []() {
        static int new_battery_level = 100;
        static bool new_battery_charging = false;
        if (new_battery_charging) {
          new_battery_level += 10;
        } else {
          new_battery_level -= 10;
        }
        if (new_battery_level > 100) {
          new_battery_level = 100;
          new_battery_charging = false;
        } else if (new_battery_level < 0) {
          new_battery_level = 0;
          new_battery_charging = true;
        }
        BatteryInfo bi {
          .level = (uint8_t)new_battery_level,
          .charging = new_battery_charging,
        };
        std::vector<uint8_t> buffer;
        espp::serialize(bi, buffer);
        espp::EventManager::get().publish(battery_topic, buffer);
        // don't want to stop the timer
        return false;
      },
      .log_level = espp::Logger::Verbosity::WARN});

  fmt::print("initializing gui...\n");
  // initialize the gui
  Gui gui({
      .play_haptic = play_haptic,
      .set_waveform = set_waveform,
      .display = display,
      .log_level = espp::Logger::Verbosity::WARN
    });

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

    auto maybe_selected_rom = gui.get_selected_rom();
    if (maybe_selected_rom.has_value()) {
      auto selected_rom = *maybe_selected_rom.value();
      fmt::print("Selected rom:\n");
      fmt::print("  {}\n", selected_rom);

      // Cart handles platform specific code, state management, etc.
      {
        print_heap_state();

        std::unique_ptr<Cart> cart(make_cart(selected_rom));

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

    fmt::print("Resuming your regularly scheduled programming...\n");

    fmt::print("During emulation, minimum free heap: {}\n", heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));

    // need to reset to control the whole screen
    espp::St7789::clear(0,0,320,240);

    gui.resume();
    display->force_refresh();
    display->resume();
  }
}
