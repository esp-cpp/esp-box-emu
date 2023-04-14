#pragma once

#include <atomic>

enum class VideoSetting { ORIGINAL, FIT, FILL, MAX_UNUSED };

VideoSetting get_video_setting();
void set_video_setting(const VideoSetting& setting);
