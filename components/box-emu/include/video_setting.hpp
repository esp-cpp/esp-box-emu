#pragma once

#include "format.hpp"

enum class VideoSetting { ORIGINAL, FIT, FILL, MAX_UNUSED };

// for libfmt printing of VideoSetting
template <>
struct fmt::formatter<VideoSetting> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(VideoSetting c, FormatContext& ctx) const {
    std::string name;
    switch (c) {
      case VideoSetting::ORIGINAL:
        name = "ORIGINAL";
        break;
      case VideoSetting::FIT:
        name = "FIT";
        break;
      case VideoSetting::FILL:
        name = "FILL";
        break;
      default:
        name = "UNKNOWN";
        break;
    }
    return fmt::formatter<std::string>::format(name, ctx);
  }
};
