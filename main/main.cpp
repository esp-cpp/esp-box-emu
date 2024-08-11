#include <sdkconfig.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include "drv2605.hpp"
#include "logger.hpp"
#include "task_monitor.hpp"
#include "timer.hpp"

#include "box-emu.hpp"
#include "carts.hpp"
#include "gui.hpp"
#include "heap_utils.hpp"
#include "rom_info.hpp"
#include "statistics.hpp"

using namespace std::chrono_literals;

extern "C" void app_main(void) {
  espp::Logger logger({.tag = "esp-box-emu", .level = espp::Logger::Verbosity::DEBUG});
  logger.info("Bootup");

  // initialize the hardware abstraction layer
  BoxEmu &emu = BoxEmu::get();
  emu.set_log_level(espp::Logger::Verbosity::DEBUG);
  espp::EspBox &box = espp::EspBox::get();
  logger.info("Running on {}", box.box_type());
  logger.info("Box Emu version: {}", emu.version());

  // initialize
  if (!emu.initialize_box()) {
    logger.error("Failed to initialize box!");
    return;
  }

  if (!emu.initialize_sdcard()) {
    logger.warn("Failed to initialize SD card!");
    logger.warn("This may happen if the SD card is not inserted.");
  }

  if (!emu.initialize_memory()) {
    logger.error("Failed to initialize memory!");
    return;
  }

  if (!emu.initialize_gamepad()) {
    logger.warn("Failed to initialize gamepad!");
    logger.warn("This may happen if the gamepad is not connected.");
  }

  if (!emu.initialize_battery()) {
    logger.warn("Failed to initialize battery!");
    logger.warn("This may happen if the battery is not connected.");
  }

  if (!emu.initialize_video()) {
    logger.error("Failed to initialize video!");
    return;
  }

  std::error_code ec;

  auto &external_i2c = emu.external_i2c();
  espp::Drv2605 haptic_motor(espp::Drv2605::Config{
      .device_address = espp::Drv2605::DEFAULT_ADDRESS,
      .write = std::bind(&espp::I2c::write, &external_i2c, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      .read_register = std::bind(&espp::I2c::read_at_register, &external_i2c, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
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
    haptic_motor.set_waveform(0, (espp::Drv2605::Waveform)(waveform), ec);
    haptic_motor.set_waveform(1, espp::Drv2605::Waveform::END, ec);
  };

  logger.info("initializing gui...");

  auto display = box.display();

  // initialize the gui
  Gui gui({
      .play_haptic = play_haptic,
      .set_waveform = set_waveform,
      .log_level = espp::Logger::Verbosity::DEBUG
    });

  print_heap_state();

  while (true) {
    // reset gui ready to play and user_quit
    gui.ready_to_play(false);
    while (!gui.ready_to_play()) {
      std::this_thread::sleep_for(50ms);
    }

    // have broken out of the loop, let the user know we're processing...
    haptic_motor.start(ec);

    // Now pause the LVGL gui
    gui.pause();
    display->pause();

    auto maybe_selected_rom = gui.get_selected_rom();
    if (maybe_selected_rom.has_value()) {
      auto selected_rom = *maybe_selected_rom.value();
      logger.info("Selected rom:\n\t{}", selected_rom);

      print_heap_state();

      // Cart handles platform specific code, state management, etc.
      {
        std::unique_ptr<Cart> cart(make_cart(selected_rom, display));

        if (cart) {
          while (cart->run());
        } else {
          logger.error("Failed to create cart!");
        }
      }
    } else {
      logger.error("Invalid rom selected!");
    }

    // print the frame statistics from the previous run
    print_statistics();

    logger.info("Done playing, resuming gui...");

    logger.debug("Task table:\n{}", espp::TaskMonitor::get_latest_info_table());

    // need to reset to control the whole screen
    espp::St7789::clear(0,0,320,240);

    gui.resume();
    display->force_refresh();
    display->resume();
  }
}
