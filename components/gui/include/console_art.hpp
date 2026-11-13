#pragma once

#include <string>

#include "lvgl.h"

#include "rom_info.hpp"

/// Draw a small icon of `info`'s console (a gamepad, a handheld, a disc...)
/// out of plain LVGL objects into a new `h * 8 / 5` x `h` container in
/// `parent`. Generic shapes in the console's accent color; no logos.
lv_obj_t *console_icon_create(lv_obj_t *parent, const ConsoleInfo &info, int h);

/// A rounded badge in the console's accent color: the icon and the console's
/// name, `h` pixels tall, sized to its content.
lv_obj_t *console_badge_create(lv_obj_t *parent, const ConsoleInfo &info, int h, const lv_font_t *font);

/// The SD card art for `info`, if there is any: `<root>/boxart/consoles/<key>.jpg`.
std::string console_art_path(const std::string &root, const ConsoleInfo &info);
