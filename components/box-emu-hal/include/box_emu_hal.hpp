#pragma once

#include <sdkconfig.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_system.h>
#include <esp_check.h>
#include <hal/spi_types.h>

#include "aw9523.hpp"
#include "event_manager.hpp"
#include "display.hpp"
#include "i2c.hpp"
#include "keypad_input.hpp"
#include "logger.hpp"
#include "max1704x.hpp"
#include "mcp23x17.hpp"
#include "oneshot_adc.hpp"
#include "serialization.hpp"
#include "st7789.hpp"
#include "task.hpp"
#include "timer.hpp"
#include "touchpad_input.hpp"

#if CONFIG_HARDWARE_BOX
#include "box.hpp"
#elif CONFIG_HARDWARE_BOX_3
#include "box_3.hpp"
#else
#error "Invalid module selection"
#endif

#include "emu_v0.hpp"
#include "emu_v1.hpp"

#include "es7210.hpp"
#include "es8311.hpp"
#include "lvgl_inputs.h"
#include "make_color.h"

#include "battery_info.hpp"
#include "fs_init.hpp"
#include "hal_events.hpp"
#include "input_state.hpp"
#include "mmap.hpp"
#include "statistics.hpp"
#include "usb.hpp"
#include "video_setting.hpp"

namespace hal {
  // top level init function
  void init();

  // i2c / peripheral interfaces
  void i2c_init();
  std::shared_ptr<espp::I2c> get_internal_i2c();
  std::shared_ptr<espp::I2c> get_external_i2c();

  // input
  void init_input();
  void get_input_state(InputState *state);

  // video
  static constexpr int NUM_ROWS_IN_FRAME_BUFFER = 50;
  std::shared_ptr<espp::Display> get_display();
  uint16_t *get_vram0();
  uint16_t *get_vram1();
  uint8_t *get_frame_buffer0();
  uint8_t *get_frame_buffer1();
  void lcd_write(const uint8_t *data, size_t length, uint32_t user_data);
  void lcd_write_frame(const uint16_t x, const uint16_t y, const uint16_t width, const uint16_t height, const uint8_t *data);
  void lcd_send_lines(int xs, int ys, int xe, int ye, const uint8_t *data, uint32_t user_data);
  void lcd_init();
  void set_display_brightness(float brightness);
  float get_display_brightness();

  void init_video_task();
  void set_display_size(size_t width, size_t height);
  void set_native_size(size_t width, size_t height, int pitch = -1);
  void set_palette(const uint16_t* palette, size_t size = 256);
  void push_frame(const void* frame);

  VideoSetting get_video_setting();
  void set_video_setting(const VideoSetting& setting);

  // audio

  // NOTE: changing the audio sample rate doesn't seem to affect the NES
  // emulator behavior or audio, but does affect the performance of the GB/C
  // emulator. Rates faster than this cause the GBC emulator to run fast and
  // increase the pitch of the sound, while rates lower than this cause it to
  // run slow and decrease the pitch of the sound.
  static constexpr int AUDIO_SAMPLE_RATE = 32000;
  static constexpr int AUDIO_BUFFER_SIZE = AUDIO_SAMPLE_RATE / 5;
  static constexpr int AUDIO_SAMPLE_COUNT = AUDIO_SAMPLE_RATE / 60;
  void audio_init();
  int16_t* get_audio_buffer();

  void play_audio(const uint8_t *data, uint32_t num_bytes);

  bool is_muted();
  void set_muted(bool mute);

  int get_audio_volume();
  void set_audio_volume(int percent);

  // battery
  void battery_init();
  std::shared_ptr<espp::Max1704x> get_battery();
}
