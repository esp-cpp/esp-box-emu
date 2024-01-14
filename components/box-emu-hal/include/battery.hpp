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


// for printing battery info using libfmt
template <>
struct fmt::formatter<BatteryInfo> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const BatteryInfo& info, FormatContext& ctx) {
    return fmt::format_to(ctx.out(), "BatteryInfo {{ level: {}, charge_rate: {} }}", info.level, info.charge_rate);
  }
};
