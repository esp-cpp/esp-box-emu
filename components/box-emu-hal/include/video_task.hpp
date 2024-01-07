#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "format.hpp"
#include "spi_lcd.h"
#include "task.hpp"

namespace hal {
  void init_video_task();
  void set_display_size(size_t width, size_t height);
  void set_native_size(size_t width, size_t height, int pitch = -1);
  void set_palette(const uint16_t* palette, size_t size = 256);
  void push_frame(const void* frame);
}  // namespace hal
