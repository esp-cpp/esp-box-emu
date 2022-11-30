#pragma once

#include "format.hpp"

#include "esp_heap_caps.h"
void print_heap_state() {
  fmt::print("          Biggest /     Free /    Total\n"
             "DRAM  : [{:8d} / {:8d} / {:8d}]\n"
             "PSRAM : [{:8d} / {:8d} / {:8d}]\n",
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
             heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
}
