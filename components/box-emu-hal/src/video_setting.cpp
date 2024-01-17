#include "box_emu_hal.hpp"

static std::atomic<VideoSetting> video_setting_{VideoSetting::ORIGINAL};

VideoSetting hal::get_video_setting() {
  return video_setting_;
}

void hal::set_video_setting(const VideoSetting& setting) {
  video_setting_ = setting;
}
