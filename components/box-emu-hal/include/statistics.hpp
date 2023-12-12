#pragma once

#include <algorithm>
#include <atomic>

void update_frame_time(float frame_time);
void reset_frame_time();

float get_fps();
float get_frame_time();
float get_frame_time_max();
float get_frame_time_min();
float get_frame_time_avg();
