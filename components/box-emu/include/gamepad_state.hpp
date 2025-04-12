#pragma once

#include <cstdint>

struct GamepadState {
  enum class Button {
    ANY = -1,
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    SELECT = 4,
    START = 5,
    UP = 6,
    DOWN = 7,
    LEFT = 8,
    RIGHT = 9
  };

  union {
    struct {
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
    uint16_t buttons{0};
  };

  bool is_pressed(Button button) const {
    switch (button) {
      case Button::ANY: return buttons != 0;
      case Button::A: return a;
      case Button::B: return b;
      case Button::X: return x;
      case Button::Y: return y;
      case Button::SELECT: return select;
      case Button::START: return start;
      case Button::UP: return up;
      case Button::DOWN: return down;
      case Button::LEFT: return left;
      case Button::RIGHT: return right;
      default: return false;
    }
  }

  bool operator==(const GamepadState& other) const {
    return buttons == other.buttons;
  }
  bool operator!=(const GamepadState& other) const {
    return !(*this == other);
  }
};
