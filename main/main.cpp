#include "sdkconfig.h"

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

#include "spi_lcd.h"
#include "format.hpp"
#include "st7789.hpp"
#include "udp_socket.hpp"
#include "wifi_sta.hpp"
#include "task_monitor.hpp"

#include "button_handlers.hpp"
#include "fs_init.hpp"
#include "gui.hpp"
#include "mmap.hpp"
#include "lv_port_fs.h"

extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

const char* getfield(char* line, int num) {
    const char* tok;
    for (tok = strtok(line, ",");
            tok && *tok;
            tok = strtok(NULL, ",\n"))
    {
        if (!--num)
            return tok;
    }
    return NULL;
}

#include "esp_heap_caps.h"
void print_heap_state() {
  static char buffer[128];
  sprintf(buffer,
          "          Biggest /     Free /    Total\n"
          "DRAM  : [%8d / %8d / %8d]\n"
          "PSRAM : [%8d / %8d / %8d]",
          heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
          heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
          heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
          heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
          heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
          heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
  fmt::print("{}\n", buffer);
}

enum class Emulator { UNKNOWN, NES, GAMEBOY, GAMEBOY_COLOR, SEGA_MASTER_SYSTEM, GENESIS, SNES };

struct RomInfo {
  std::string name;
  std::string boxart_path;
  std::string rom_path;
  Emulator platform;
};

const std::string WHITESPACE = " \n\r\t\f\v";

std::string ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string &s) {
    return rtrim(ltrim(s));
}

std::vector<RomInfo> parse_metadata(const std::string& metadata_path) {
  std::vector<RomInfo> infos;
  // load metadata path
  std::ifstream metadata(metadata_path, std::ios::in);
  if (!metadata.is_open()) {
    fmt::print("Couldn't load metadata file {}!\n", metadata_path);
    return infos;
  }
  // parse it as csv, format = rom_path, boxart_path, name - name is last
  // because it might have commas in it.
  std::string line;
  while (std::getline(metadata, line)) {
    // get the fields from each line
    std::string rom_path, boxart_path, name;
    char *str = line.data();
    char *token = strtok(str, ",");
    int num_tokens = 0;
    while (token != NULL && num_tokens < 3) {
      switch (num_tokens) {
      case 0:
        // rom path
        rom_path = token;
        rom_path = trim(rom_path);
        break;
      case 1:
        // boxart path
        boxart_path = token;
        boxart_path = trim(boxart_path);
        break;
      case 2:
        // name
        name = token;
        name = trim(name);
        break;
      default:
        // DANGER WILL ROBINSON
        break;
      }
      token = strtok(NULL, ",");
      num_tokens++;
    }
    fmt::print("INFO: '{}', '{}', '{}'\n", rom_path, boxart_path, name);
    // for each row, create rom entry
    infos.emplace_back(name, boxart_path, rom_path, Emulator::NES);
  }

  return infos;
}

extern "C" void app_main(void) {
  fmt::print("Starting esp-box-emu...\n");

  // init nvs and partition
  init_memory();
  // init filesystem
  fs_init();
  // init lv connection to filesystem
  // init the display subsystem
  lcd_init();

  // initialize the gui
  Gui gui({
      .display = display
    });

  // Create the input devices used for the NES emulator
  InputDevice joypad0(-1);
  InputDevice joypad1(-2);

  std::atomic<bool> quit{false};

  espp::WifiSta wifi_sta({
      .ssid = CONFIG_ESP_WIFI_SSID,
        .password = CONFIG_ESP_WIFI_PASSWORD,
        .num_connect_retries = CONFIG_ESP_MAXIMUM_RETRY,
        .on_connected = nullptr,
        .on_disconnected = nullptr,
        .on_got_ip = [](ip_event_got_ip_t* eventdata) {
          fmt::print("got IP: {}.{}.{}.{}\n", IP2STR(&eventdata->ip_info.ip));
        }
        });

  size_t port = 5000;
  espp::UdpSocket server_socket({.log_level=espp::Logger::Verbosity::WARN});
  auto server_task_config = espp::Task::Config{
    .name = "JoypadServer",
    .callback = nullptr,
    .stack_size_bytes = 6 * 1024,
  };
  auto server_config = espp::UdpSocket::ReceiveConfig{
    .port = port,
    .buffer_size = 1024,
    .on_receive_callback = [&joypad0, &quit](auto& data, auto& source) -> auto {
      bool wants_to_quit = handle_input_report(data, &joypad0);
      if (wants_to_quit) {
        quit = true;
      }
      return std::nullopt;
    }
  };
  server_socket.start_receiving(server_task_config, server_config);

  fmt::print("{}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();

  fmt::print("Setting rom. LV_USE_SJPG = {}\n", LV_USE_SJPG);

  fmt::print("initializing the lv FS port...\n");

  std::this_thread::sleep_for(1s);
  lv_port_fs_init();

  // load the metadata.csv file, parse it, and add roms from it
  auto roms = parse_metadata("/littlefs/metadata.csv");
  std::string boxart_prefix = "L:";
  for (auto& rom : roms) {
    gui.add_rom(rom.name, boxart_prefix + rom.boxart_path);
  }
  while (true) {
    // scroll through the rom list forever :)
    gui.next();
    std::this_thread::sleep_for(5s);
  }

  // Now pause the LVGL gui
  display->pause();
  gui.pause();

  fmt::print("{}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();

  // ensure the display has been paused
  std::this_thread::sleep_for(100ms);

  // Clear the display
  espp::St7789::clear(0,0,320,240);
  int x_offset, y_offset;
  // set the offset (to center the emulator output)
  espp::St7789::get_offset(x_offset, y_offset);
  // WIDTH = 256, so 320-WIDTH 64
  espp::St7789::set_offset((320-256) / 2, 0);

  std::this_thread::sleep_for(1s);

  // copy the rom into the nesgame partition and memory map it
  std::string rom_filename = "/littlefs/zelda.nes";
  bool did_copy = copy_romdata_to_nesgame_partition(rom_filename);
  while (!did_copy) {
    fmt::print("Could not copy {} into nesgame_partition!\n", rom_filename);
    std::this_thread::sleep_for(10s);
  }
  uint8_t* romdata = get_mmapped_romdata();

  // now start the emulator
  Gamepak gamepak(rom_filename);
  gamepak.initialize(romdata);
  PPU ppu(&gamepak, display->vram());
  fmt::print("ppu: {}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();
  NesCPUMemory cpuMemory(&ppu, &gamepak, &joypad0, &joypad1);
  fmt::print("memory: {}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();
  NesCpu cpu(&cpuMemory);
  fmt::print("cpu: {}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();
  ppu.assign_cpu(&cpu);
  cpu.power_up();
  ppu.power_up();

  fmt::print("{}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();

  uint32_t cpu_counter = 7;
  uint32_t ppu_counter = 0;
  while (!quit) {
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

  fmt::print("{}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();

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
