#include "nes.hpp"
#include "gameboy.hpp"

#include "sdkconfig.h"

#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/queue.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include "i2c.hpp"
#include "input.h"
#include "i2s_audio.h"
#include "spi_lcd.h"
#include "format.hpp"
#include "st7789.hpp"
#include "task_monitor.hpp"

#include "drv2605.hpp"

#include "cart.hpp"
#include "heap_utils.hpp"
#include "string_utils.hpp"
#include "fs_init.hpp"
#include "gui.hpp"
#include "mmap.hpp"
#include "rom_info.hpp"

// from spi_lcd.cpp
extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

static QueueHandle_t gpio_evt_queue;
static void gpio_isr_handler(void *arg) {
  uint32_t gpio_num = (uint32_t)arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
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

  auto write_drv = [](uint8_t reg, uint8_t data) {
    uint8_t buf[] = {reg, data};
    i2c_write_external_bus(Drv2605::ADDRESS, buf, 2);
  };
  auto read_drv = [](uint8_t reg) -> uint8_t {
    uint8_t read_data;
    i2c_read_external_bus(Drv2605::ADDRESS, reg, &read_data, 1);
    return read_data;
  };

  Drv2605 haptic_motor({
      .write = write_drv,
      .read = read_drv,
    });
  // we're using an ERM motor, so select an ERM library.
  haptic_motor.select_library(1);

  auto play_haptic = [&haptic_motor]() {
    haptic_motor.start();
  };
  auto set_waveform = [&haptic_motor](int waveform) {
    haptic_motor.set_waveform(0, (Drv2605::Waveform)waveform);
    haptic_motor.set_waveform(1, Drv2605::Waveform::END);
  };

  fmt::print("initializing gui...\n");
  // initialize the gui
  Gui gui({
      .play_haptic = play_haptic,
      .set_waveform = set_waveform,
      .display = display,
      .log_level = espp::Logger::Verbosity::WARN
    });

  static constexpr size_t BOOT_GPIO = 0;
  static constexpr size_t MUTE_GPIO = 1;
  // create the gpio event queue
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  // setup gpio interrupts for boot button and mute button
  gpio_config_t io_conf;
  memset(&io_conf, 0, sizeof(io_conf));
  // interrupt on any edge (since MUTE is connected to flipflop, see note below)
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  io_conf.pin_bit_mask = (1<<MUTE_GPIO) | (1<<BOOT_GPIO);
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);
  //install gpio isr service
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)BOOT_GPIO, gpio_isr_handler, (void*) BOOT_GPIO);
  gpio_isr_handler_add((gpio_num_t)MUTE_GPIO, gpio_isr_handler, (void*) MUTE_GPIO);
  // start the gpio task
  espp::Task gpio_task(espp::Task::Config{
      .name = "gbc task",
      .callback = [&gui](auto &m, auto&cv) {
        static uint32_t io_num;
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
          // invert the state since these are active low switches
          bool pressed = !gpio_get_level((gpio_num_t)io_num);
          // fmt::print("Got gpio interrupt: {}, pressed: {}\n", io_num, pressed);
          // see if it's the mute button or the boot button
          if (io_num == MUTE_GPIO) {
            // NOTE: the MUTE is actually connected to a flip-flop which holds
            // state, so pressing it actually toggles the state that we see on
            // the ESP pin. Therefore, when we get an edge trigger, we should
            // read the state to know whether to be muted or not.
            gui.set_mute(pressed);
            // now make sure the output sound is updated
            set_audio_volume(gui.get_audio_level());
          } else if (io_num == BOOT_GPIO && pressed) {
            // toggle between the original / fit / fill video scaling modes;
            // NOTE: only do something the state is high, since this gpio has no
            // flip-flop.
            gui.next_video_setting();
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
            case Gui::VideoSetting::MAX_UNUSED:
            default:
              break;
            }
          }
        }
      },
      .stack_size_bytes = 4*1024,
    });
  gpio_task.start();

  // update the mute state (since it's a flip-flop and may have been set if we
  // restarted without power loss)
  bool muted = !gpio_get_level((gpio_num_t)MUTE_GPIO);
  gui.set_mute(muted);

  // the prefix for the filesystem (either littlefs or sdcard)
  std::string fs_prefix = MOUNT_POINT;

  // load the metadata.csv file, parse it, and add roms from it
  auto roms = parse_metadata(fs_prefix + "/metadata.csv");
  std::string boxart_prefix = fs_prefix + "/";
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

    // have broken out of the loop, let the user know we're processing...
    haptic_motor.start();

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
    case Gui::VideoSetting::MAX_UNUSED:
    default:
      break;
    }

    // ensure the display has been paused
    std::this_thread::sleep_for(500ms);

    auto selected_rom_index = gui.get_selected_rom_index();
    fmt::print("Selected rom index: {}\n", selected_rom_index);
    auto selected_rom_info = roms[selected_rom_index];

    // Cart handles platform specific code, state management, etc.
    {
      Cart cart(selected_rom_info);
      cart.load();
      cart.run();
      cart.save();
    }

    // TODO: move the save state / slot mangagement into this component - should
    //       probably define how many (if limited) slots are available per game.
    //       Alternatively, might be easier (assumign the card supports it) to
    //       simply create all slots within a folder of the same name as the
    //       cart itself.

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
