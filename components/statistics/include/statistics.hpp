#pragma once

#include <algorithm>
#include <atomic>

#include "format.hpp"

void update_frame_time(uint64_t frame_time);
void reset_frame_time();

float get_fps();
uint64_t get_frame_time();
uint64_t get_frame_time_max();
uint64_t get_frame_time_min();
float get_frame_time_avg();

void print_statistics();
