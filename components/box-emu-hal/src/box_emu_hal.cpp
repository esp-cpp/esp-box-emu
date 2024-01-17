#include "box_emu_hal.hpp"

void hal::init() {
  // init nvs and partition
  init_memory();
  // init filesystem
  fs_init();
  // init the display subsystem
  hal::lcd_init();
  // initialize the i2c buses for touchpad, imu, audio codecs, gamepad, haptics, etc.
  hal::i2c_init();
  // init the audio subsystem
  hal::audio_init();
  // init the input subsystem
  hal::init_input();
  // initialize the video task for the emulators
  hal::init_video_task();
  // initialize the battery system
  hal::battery_init();
}
