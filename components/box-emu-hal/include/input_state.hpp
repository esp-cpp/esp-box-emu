#pragma once

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
