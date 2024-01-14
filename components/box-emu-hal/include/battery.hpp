#pragma once

#include "event_manager.hpp"
#include "max1704x.hpp"
#include "serialization.hpp"

#include "hal_i2c.hpp"

static const std::string battery_topic = "battery";

struct BatteryInfo {
  float level;
  float charge_rate;
};

namespace hal {
  void battery_init();
  std::shared_ptr<espp::Max1704x> get_battery();
} // namespace hal
