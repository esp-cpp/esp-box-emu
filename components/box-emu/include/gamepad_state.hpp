#pragma once

struct GamepadState {
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

  bool operator==(const GamepadState& other) const = default;
};
