#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

  lv_indev_t *get_keypad_input_device();
  void touchpad_read(unsigned char *num_touch_points, unsigned short* x, unsigned short* y, unsigned char* btn_state);

#ifdef __cplusplus
}

#endif
