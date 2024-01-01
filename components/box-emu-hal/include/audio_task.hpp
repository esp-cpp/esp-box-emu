#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "format.hpp"
#include "i2s_audio.h"
#include "task.hpp"

namespace hal {
  void init_audio_task();
  void set_audio_sample_count(size_t count);
  void push_audio(const void* audio);
}  // namespace hal
