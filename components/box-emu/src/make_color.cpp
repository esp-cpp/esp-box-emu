#include "make_color.h"

#include <lvgl.h>

extern "C" uint16_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    return lv_color_to_u16(lv_color_make(r, g, b));
}
