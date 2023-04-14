#include "video_setting.hpp"

static std::atomic<VideoSetting> video_setting_;

VideoSetting get_video_setting() {
  return video_setting_;
}

void set_video_setting(const VideoSetting& setting) {
  video_setting_ = setting;
}
