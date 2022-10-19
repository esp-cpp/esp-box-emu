#pragma once

#include "joypad_buttons.hpp"
#include "InputDevice.h"

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
