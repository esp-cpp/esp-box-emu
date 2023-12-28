#pragma once

#include "serialization.hpp"

static const std::string battery_topic = "battery";

struct BatteryInfo {
  uint8_t level;
  bool charging;
};
