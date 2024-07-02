#pragma once

#include "format.hpp"

struct BatteryInfo {
  float voltage;
  float level;
  float charge_rate;
};

// for printing battery info using libfmt
template <>
struct fmt::formatter<BatteryInfo> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const BatteryInfo& info, FormatContext& ctx) {
    return fmt::format_to(ctx.out(), "BatteryInfo {{ voltage: {:.1f}, level: {:.1f}, charge_rate: {:.1f} }}", info.voltage, info.level, info.charge_rate);
  }
};
