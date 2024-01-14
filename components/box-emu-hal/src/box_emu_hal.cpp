#include "box_emu_hal.hpp"

void hal::init() {
  // init nvs and partition
  init_memory();
  // init filesystem
  fs_init();
  // init the display subsystem
  lcd_init();
  // initialize the i2c buses for touchpad, imu, audio codecs, gamepad, haptics, etc.
  i2c_init();
  // init the audio subsystem
  audio_init();
  // init the input subsystem
  init_input();
  // initialize the video task for the emulators
  hal::init_video_task();
  // initialize the audio task for the emulators
  hal::init_audio_task();
  // initialize the battery system
  hal::battery_init();
}
