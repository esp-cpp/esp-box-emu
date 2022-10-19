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

extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

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

  // Run the LVGL gui
  size_t iterations = 0;
  size_t max_iterations = 20;
  while (iterations < max_iterations) {
    auto label = fmt::format("Iterations: {}", iterations);
    gui.set_label(label);
    gui.set_meter((iterations*100)/max_iterations);
    iterations++;
    std::this_thread::sleep_for(100ms);
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
  copy_romdata_to_nesgame_partition(rom_filename);
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
    auto label = fmt::format("Iterations: {}", iterations);
    gui.set_label(label);
    gui.set_meter(iterations % 100);
    iterations++;
    std::this_thread::sleep_for(100ms);
  }
}
