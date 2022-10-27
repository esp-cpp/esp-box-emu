#pragma once

#include <vector>

#include "format.hpp"

#include "joypad_buttons.hpp"
#include "nes_lib/InputDevice.h"

class Report {
public:
  enum class Field {
    TYPE,
    LEFT_ANALOG_X,
    LEFT_ANALOG_Y,
    RIGHT_ANALOG_X,
    RIGHT_ANALOG_Y,
    BUTTON_CODE_1,
    BUTTON_CODE_2,
    UNKNOWN,
    LEFT_ANALOG_TRIGGER,
    RIGHT_ANALOG_TRIGGER,
  };

  enum class ButtonCode1 {
    Y = 4, // left
    B = 5, // down
    A = 6, // right
    X = 7, // up
  };
  enum class ButtonCode2 {
    L1 = 0,
    R1 = 1,
    L2 = 2,
    R2 = 3,
    SELECT = 4,
    START = 5,
  };

  Report(const std::vector<uint8_t>& data) {
    left_stick_x = parse_analog(data[(int)Field::LEFT_ANALOG_X]);
    left_stick_y = parse_analog(data[(int)Field::LEFT_ANALOG_Y]);
    right_stick_x = parse_analog(data[(int)Field::RIGHT_ANALOG_X]);
    right_stick_y = parse_analog(data[(int)Field::RIGHT_ANALOG_Y]);

    uint8_t button_code_1 = data[(int)Field::BUTTON_CODE_1];
    decode_dpad(button_code_1);
    a = button_code_1 & (1 << (int)ButtonCode1::A);
    b = button_code_1 & (1 << (int)ButtonCode1::B);
    x = button_code_1 & (1 << (int)ButtonCode1::X);
    y = button_code_1 & (1 << (int)ButtonCode1::Y);

    uint8_t button_code_2 = data[(int)Field::BUTTON_CODE_2];
    l1 = button_code_2 & (1 << (int)ButtonCode2::L1);
    r1 = button_code_2 & (1 << (int)ButtonCode2::R1);
    l2 = button_code_2 & (1 << (int)ButtonCode2::L2);
    r2 = button_code_2 & (1 << (int)ButtonCode2::R2);
    start = button_code_2 & (1 << (int)ButtonCode2::START);
    select = button_code_2 & (1 << (int)ButtonCode2::SELECT);

    left_trigger = parse_analog(data[(int)Field::LEFT_ANALOG_TRIGGER], 0.0f, 255.0f);
    right_trigger = parse_analog(data[(int)Field::RIGHT_ANALOG_TRIGGER], 0.0f, 255.0f);
  }

  void set_joypad_state(InputDevice *joypad) {
    joypad->externState[(int)JoypadButtons::A] = a;
    joypad->externState[(int)JoypadButtons::B] = b;
    joypad->externState[(int)JoypadButtons::Select] = select;
    joypad->externState[(int)JoypadButtons::Start] = start;
    joypad->externState[(int)JoypadButtons::Up] = dpad_up;
    joypad->externState[(int)JoypadButtons::Down] = dpad_down;
    joypad->externState[(int)JoypadButtons::Left] = dpad_left;
    joypad->externState[(int)JoypadButtons::Right] = dpad_right;
  }

  void decode_dpad(uint8_t value) {
    dpad_up = dpad_down = dpad_left = dpad_right = false;
    switch (value) {
    case 0:
      dpad_up = true;
      break;
    case 1:
      dpad_up = true;
      dpad_right = true;
      break;
    case 2:
      dpad_right = true;
      break;
    case 3:
      dpad_down = true;
      dpad_right = true;
      break;
    case 4:
      dpad_down = true;
      break;
    case 5:
      dpad_down = true;
      dpad_left = true;
      break;
    case 6:
      dpad_left = true;
      break;
    case 7:
      dpad_left = true;
      dpad_up = true;
      break;
    case 8:
      // nothing is pressed
      break;
    default:
      break;
    }
  }

  static float parse_analog(uint8_t value, float center=127.0f, float range=127.0f) {
    return ((float)value - center) / range;
  }

  bool wants_to_quit() const {
    return select && start;
  }

protected:
  float left_stick_x;
  float left_stick_y;
  float right_stick_x;
  float right_stick_y;
  bool dpad_up;
  bool dpad_down;
  bool dpad_left;
  bool dpad_right;
  bool a;
  bool b;
  bool x;
  bool y;
  bool l1;
  bool r1;
  bool l2;
  bool r2;
  bool start;
  bool select;
  float left_trigger;
  float right_trigger;
};

bool handle_input_report(const std::vector<uint8_t>& report_data, InputDevice* joypad) {
  if (report_data.size() != 10) {
    fmt::print("Bad report size {}\n", report_data.size());
    return false;
  }
  Report report(report_data);
  report.set_joypad_state(joypad);
  return report.wants_to_quit();
}

bool string_to_input(InputDevice* joypad, const std::string& strdata) {
  static JoypadButtons prev_button = JoypadButtons::NONE;
  if (strdata.find("quit") != std::string::npos) {
    fmt::print("QUITTING\n");
    return true;
  } else if (strdata.find("start") != std::string::npos) {
    fmt::print("start pressed\n");
    prev_button = JoypadButtons::Start;
  } else if (strdata.find("select") != std::string::npos) {
    fmt::print("select pressed\n");
    prev_button = JoypadButtons::Select;
  } else if (strdata.find("clear") != std::string::npos) {
    fmt::print("clearing button: {}\n", (int)prev_button);
    joypad->externState[(int)prev_button] = false;
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
    joypad->externState[(int)prev_button] = true;
  }
  return false;
}
