#include "make_color.h"

#include <lvgl/lvgl.h>

extern "C" uint16_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    return lv_color_make(r,g,b).full;
}
