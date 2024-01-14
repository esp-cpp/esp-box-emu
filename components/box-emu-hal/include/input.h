#pragma once

#include "lvgl.h"

#include "hal_i2c.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

  struct InputState {
    int a : 1;
    int b : 1;
    int x : 1;
    int y : 1;
    int select : 1;
    int start : 1;
    int up : 1;
    int down : 1;
    int left : 1;
    int right : 1;
  };

  void init_input();
  void get_input_state(struct InputState *state);
  lv_indev_t *get_keypad_input_device();
  void touchpad_read(unsigned char *num_touch_points, unsigned short* x, unsigned short* y, unsigned char* btn_state);

#ifdef __cplusplus
}

[[maybe_unused]] static bool operator==(const InputState& lhs, const InputState& rhs) {
  return
    lhs.a == rhs.a &&
    lhs.b == rhs.b &&
    lhs.x == rhs.x &&
    lhs.y == rhs.y &&
    lhs.select == rhs.select &&
    lhs.start == rhs.start &&
    lhs.up == rhs.up &&
    lhs.down == rhs.down &&
    lhs.left == rhs.left &&
    lhs.right == rhs.right;
}

#endif
