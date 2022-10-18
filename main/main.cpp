#include "utils.h"
#include "cpu.h"
#include "Gamepak.h"
#include "InputDevice.h"
#include "PPU.h"
#include "PPUMemory.h"
#include "memory.h"
#include "nes_clock.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include <esp_err.h>

#include "tinyusb.h"
#include "tusb_cdc_acm.h"

#include "esp_littlefs.h"
#include <sys/stat.h>

#include "spi_lcd.h"
#include "format.hpp"
#include "gui.hpp"
#include "st7789.hpp"

extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

static InputDevice joypad0(-1);
static InputDevice joypad1(-2);

void handle_input(char ch) {
  static int prev_btn = -1;
  if (ch > 0) {
    fmt::print("got character: '{}'\n", ch);
  } else {
    return;
  }
  switch (ch) {
  case 'w':
    // up
    prev_btn = 4;
    break;
  case 'a':
    // left
    prev_btn = 6;
    break;
  case 's':
    // down
    prev_btn = 5;
    break;
  case 'd':
    // right
    prev_btn = 7;
    break;
  case 'j':
    // a
    prev_btn = 0;
    break;
  case 'k':
    // b
    prev_btn = 1;
    break;
  case ' ':
    // select
    prev_btn = 2;
    break;
  case 'b':
    // start
    prev_btn = 3;
    break;
  case '\n': {
    // clear
    if (prev_btn != -1) {
      joypad0.externState[prev_btn] = false;
      prev_btn = -1;
    }
    break;
  }
  default:
    break;
  }
  if (prev_btn != -1) {
    joypad0.externState[prev_btn] = true;
  }
}

extern "C" void app_main(void) {
  esp_vfs_littlefs_conf_t conf = {
      .base_path = "/littlefs",
      .partition_label = "littlefs",
      .format_if_mount_failed = true,
      .dont_mount = false,
  };

  // Use settings defined above to initialize and mount LittleFS filesystem.
  // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      fmt::print("Failed to mount or format filesystem\n");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      fmt::print("Failed to find LittleFS partition\n");
    } else {
      fmt::print("Failed to initialize LittleFS ({})\n", esp_err_to_name(ret));
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_littlefs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    fmt::print("Failed to get LittleFS partition information ({})\n",
                 esp_err_to_name(ret));
  } else {
    fmt::print("Partition size: total: {}, used: {}\n", total, used);
  }

  lcd_init();
  fmt::print("Starting esp-box-emu...\n");
  // initialize the gui
  Gui gui({
      .display = display
    });
  size_t iterations = 0;
  while (iterations < 10) {
    auto label = fmt::format("Iterations: {}", iterations);
    gui.set_label(label);
    gui.set_meter(iterations*10);
    iterations++;
    std::this_thread::sleep_for(100ms);
  }

  // Now pause the LVGL gui
  display->pause();

  std::this_thread::sleep_for(100ms);

  // Clear the display
  espp::St7789::clear(0,0,320,240);
  int x_offset, y_offset;
  // set the offset (to center the emulator output)
  espp::St7789::get_offset(x_offset, y_offset);
  // WIDTH = 256, so 320-WIDTH 64
  espp::St7789::set_offset((320-256) / 2, 0);

  std::this_thread::sleep_for(1s);

  // now start the emulator
  std::string rom_filename = "/littlefs/zelda.nes";

  Gamepak gamepak(rom_filename);
  gamepak.initialize();
  PPU ppu(&gamepak);
  NesCPUMemory cpuMemory(&ppu, &gamepak, &joypad0, &joypad1);
  NesCpu cpu(&cpuMemory);
  ppu.assign_cpu(&cpu);
  cpu.power_up();
  ppu.power_up();


  const tinyusb_config_t tusb_cfg = {
    .device_descriptor = NULL,
    .string_descriptor = NULL,
    .external_phy = false,
    .configuration_descriptor = NULL,
  };

  ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

  tinyusb_config_cdcacm_t acm_cfg = {
    .usb_dev = TINYUSB_USBDEV_0,
      .cdc_port = TINYUSB_CDC_ACM_0,
      .rx_unread_buf_sz = 64,
      .callback_rx = [](int itf, cdcacm_event_t *event) {
        /* initialization */
        size_t rx_size = 0;
        /* read */
        esp_err_t ret = tinyusb_cdcacm_read((tinyusb_cdcacm_itf_t)itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
        if (ret == ESP_OK) {
          std::string recv{(const char*)buf, rx_size};
          fmt::print("read data from channel '{}'\n", (int)itf);
          fmt::print("\t'{}'\n", recv);
        } else {
          fmt::print("read error\n");
        }
        // handle_input()
    }, // &tinyusb_cdc_rx_callback, // the first way to register a callback
    .callback_rx_wanted_char = NULL,
    .callback_line_state_changed = NULL,
    .callback_line_coding_changed = NULL
  };

  ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
  /* the second way to register a callback */
  /*
  ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
                                                   TINYUSB_CDC_ACM_0,
                                                   CDC_EVENT_LINE_STATE_CHANGED,
                                                   &tinyusb_cdc_line_state_changed_callback));
  */

  uint32_t cpu_counter = 7;
  uint32_t ppu_counter = 0;
  while (true) {
    bool image_ready = false;
    bool generated_image = false;
    while (!generated_image) {
      cpu_counter += cpu.step().count();
      while (ppu_counter < cpu_counter*3) {
        image_ready = ppu.step();
        if (image_ready) {
          generated_image = true;
        }
        ppu_counter++;
      }
    }
  }

  // we shouldn't get here!
  espp::St7789::clear(0,0,320,240);
  // reset the offset
  espp::St7789::set_offset(x_offset, y_offset);
  display->resume();
  while (true) {
    auto label = fmt::format("Iterations: {}", iterations);
    gui.set_label(label);
    gui.set_meter(iterations % 100);
    iterations++;
    std::this_thread::sleep_for(100ms);
  }
}
