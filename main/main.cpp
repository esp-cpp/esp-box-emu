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

#include <esp_err.h>
#include "nvs_flash.h"

#include "spi_lcd.h"
#include "format.hpp"
#include "st7789.hpp"
#include "controller.hpp"
#include "udp_socket.hpp"
#include "wifi_sta.hpp"

#include "fs_init.hpp"
#include "gui.hpp"

extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

enum class JoypadButtons : int {
  A,
  B,
  Select,
  Start,
  Up,
  Down,
  Left,
  Right,
  NONE
};

extern "C" void app_main(void) {
  fmt::print("Starting esp-box-emu...\n");

  // Initialize NVS, needed for BT
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    fmt::print("Erasing NVS flash...\n");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

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

  fs_init();
  lcd_init();

  // initialize the gui
  Gui gui({
      .display = display
    });

  // Create the input devices used for the NES emulator
  InputDevice joypad0(-1);
  InputDevice joypad1(-2);

  std::atomic<bool> quit{false};

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
      fmt::print("Server {} bytes from source: {}:{}\n",
                 data.size(), source.address, source.port);
      std::string strdata(data.begin(), data.end());
      static JoypadButtons prev_button = JoypadButtons::NONE;
      if (strdata.find("quit") != std::string::npos) {
        fmt::print("QUITTING\n");
        quit = true;
      } else if (strdata.find("start") != std::string::npos) {
        fmt::print("start pressed\n");
        prev_button = JoypadButtons::Start;
      } else if (strdata.find("select") != std::string::npos) {
        fmt::print("select pressed\n");
        prev_button = JoypadButtons::Select;
      } else if (strdata.find("clear") != std::string::npos) {
        fmt::print("clearing button: {}\n", (int)prev_button);
        joypad0.externState[(int)prev_button] = false;
        prev_button = JoypadButtons::NONE;
      } else if (strdata.find("a") != std::string::npos) {
        fmt::print("A pressed\n");
        prev_button = JoypadButtons::A;
      } else if (strdata.find("b") != std::string::npos) {
        fmt::print("B pressed\n");
        prev_button = JoypadButtons::B;
      } else if (strdata.find("up") != std::string::npos) {
        fmt::print("Up pressed\n");
        prev_button = JoypadButtons::Up;
      } else if (strdata.find("down") != std::string::npos) {
        fmt::print("Down pressed\n");
        prev_button = JoypadButtons::Down;
      } else if (strdata.find("left") != std::string::npos) {
        fmt::print("Left pressed\n");
        prev_button = JoypadButtons::Left;
      } else if (strdata.find("right") != std::string::npos) {
        fmt::print("Right pressed\n");
        prev_button = JoypadButtons::Right;
      }
      if (prev_button != JoypadButtons::NONE) {
        joypad0.externState[(int)prev_button] = true;
      }
      return std::nullopt;
    }
  };
  server_socket.start_receiving(server_task_config, server_config);

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

  // now start the emulator
  std::string rom_filename = "/littlefs/zelda.nes";

  Gamepak gamepak(rom_filename);
  gamepak.initialize();
  PPU ppu(&gamepak, display->vram());
  NesCPUMemory cpuMemory(&ppu, &gamepak, &joypad0, &joypad1);
  NesCpu cpu(&cpuMemory);
  ppu.assign_cpu(&cpu);
  cpu.power_up();
  ppu.power_up();

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
