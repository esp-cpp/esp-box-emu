#include "Emulator.hpp"
#include "spi_lcd.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <esp_err.h>

#include "esp_littlefs.h"
#include <sys/stat.h>

#include "format.hpp"
#include "gui.hpp"
#include "st7789.hpp"

extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

extern "C" char *osd_getromdata(char* filename) {
  fmt::print("osd_getromdata\n");
  std::string fname = "/littlefs/";
  fname += filename;
  struct stat st;
  stat(fname.c_str(), &st);
  size_t size = st.st_size;
  fmt::print("ROM '{}' size: {}\n", fname, size);
  FILE* f = fopen(fname.c_str(), "r");
  if (f == NULL) {
    fmt::print("Failed to open file '{}' for reading\n", fname);
    return nullptr;
  }
	char *romdata = new char[size];
  fseek(f, 0, SEEK_SET);
  fread(romdata, size, 1, f);
  fclose(f);
  return (char*)romdata;
}


extern "C" esp_err_t event_handler(void *ctx, void *event)
{
    return ESP_OK;
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
  espp::St7789::set_offset((320-WIDTH) / 2, 0);

  std::this_thread::sleep_for(1s);

  // now start the emulator
  const char* rom_filename = "/littlefs/rom.nes";
  Emulator *emu = new Emulator(rom_filename);
  emu->Execute();

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
