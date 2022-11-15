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
    case Gui::VideoSetting::MAX_UNUSED:
    default:
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
