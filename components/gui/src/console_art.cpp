#include "console_art.hpp"

#include <algorithm>
#include <filesystem>

namespace {

lv_obj_t *shape(lv_obj_t *parent, int x, int y, int w, int h, int radius, lv_color_t color) {
  auto *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_bg_color(o, color, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  return o;
}

// a + shaped d-pad centered at (cx, cy), `s` across
void dpad(lv_obj_t *parent, int cx, int cy, int s, lv_color_t color) {
  const int t = std::max(2, s / 3);
  shape(parent, cx - s / 2, cy - t / 2, s, t, 1, color);
  shape(parent, cx - t / 2, cy - s / 2, t, s, 1, color);
}

void dot(lv_obj_t *parent, int cx, int cy, int d, lv_color_t color) {
  shape(parent, cx - d / 2, cy - d / 2, d, d, LV_RADIUS_CIRCLE, color);
}

} // namespace

lv_obj_t *console_icon_create(lv_obj_t *parent, const ConsoleInfo &info, int h) {
  const int w = h * 8 / 5;
  auto *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(box, w, h);

  const lv_color_t body = lv_color_white();
  const lv_color_t accent = lv_color_hex(info.color);
  const lv_color_t dark = lv_color_hex(0x202428);

  switch (info.shape) {
  case ConsoleShape::GAMEPAD: {
    // a wide rounded controller: d-pad left, two buttons right
    const int bh = h * 6 / 10, by = (h - bh) / 2;
    shape(box, 0, by, w, bh, bh / 2, body);
    dpad(box, w * 27 / 100, h / 2, bh * 6 / 10, accent);
    dot(box, w * 66 / 100, h / 2 + bh / 8, bh / 4, accent);
    dot(box, w * 80 / 100, h / 2 - bh / 8, bh / 4, accent);
    break;
  }
  case ConsoleShape::HANDHELD_TALL: {
    // a portrait handheld: screen on top, d-pad and buttons below it
    const int bw = h * 66 / 100, bx = (w - bw) / 2;
    shape(box, bx, 0, bw, h, bw / 6, body);
    shape(box, bx + bw / 8, h / 10, bw * 6 / 8, h * 4 / 10, 2, dark);
    dpad(box, bx + bw * 30 / 100, h * 72 / 100, bw * 3 / 10, accent);
    dot(box, bx + bw * 68 / 100, h * 76 / 100, bw / 6, accent);
    dot(box, bx + bw * 84 / 100, h * 66 / 100, bw / 6, accent);
    break;
  }
  case ConsoleShape::HANDHELD_WIDE: {
    // a landscape handheld: screen in the middle, d-pad and buttons beside it
    const int bh = h * 7 / 10, by = (h - bh) / 2;
    shape(box, 0, by, w, bh, bh * 4 / 10, body);
    shape(box, w * 30 / 100, by + bh / 6, w * 40 / 100, bh * 2 / 3, 2, dark);
    dpad(box, w * 15 / 100, h / 2, bh * 4 / 10, accent);
    dot(box, w * 82 / 100, h / 2 + bh / 10, bh / 5, accent);
    dot(box, w * 90 / 100, h / 2 - bh / 10, bh / 5, accent);
    break;
  }
  case ConsoleShape::DISC: {
    // a CD: rim, label ring, hole
    const int d = h, x = (w - d) / 2;
    shape(box, x, 0, d, d, LV_RADIUS_CIRCLE, body);
    dot(box, w / 2, h / 2, d * 45 / 100, accent);
    dot(box, w / 2, h / 2, d * 15 / 100, body);
    break;
  }
  case ConsoleShape::COMPUTER: {
    // a monitor on a stand
    const int mw = w * 70 / 100, mh = h * 66 / 100, mx = (w - mw) / 2;
    shape(box, mx, 0, mw, mh, 3, body);
    shape(box, mx + mw / 12, mh / 8, mw * 10 / 12, mh * 6 / 8, 1, accent);
    shape(box, w / 2 - w / 14, mh, w / 7, h / 6, 0, body);
    shape(box, w / 2 - w / 5, h - h / 8, w * 2 / 5, h / 8, 2, body);
    break;
  }
  case ConsoleShape::FANTASY: {
    // a tiny cartridge-console: a square with a screen of palette dots
    const int s = h, x = (w - s) / 2;
    shape(box, x, 0, s, s, s / 6, body);
    const int sx = x + s / 8, sy = s / 8, ss = s * 6 / 8;
    shape(box, sx, sy, ss, ss * 3 / 4, 2, dark);
    const uint32_t palette[] = {0xFF004D, 0xFFA300, 0xFFEC27, 0x00E436};
    const int p = std::max(2, ss / 6);
    for (int i = 0; i < 4; i++) {
      shape(box, sx + ss / 10 + i * (p + p / 3), sy + ss * 3 / 8 - p / 2, p, p, 0, lv_color_hex(palette[i]));
    }
    break;
  }
  }
  return box;
}

lv_obj_t *console_badge_create(lv_obj_t *parent, const ConsoleInfo &info, int h, const lv_font_t *font) {
  auto *badge = lv_obj_create(parent);
  lv_obj_remove_style_all(badge);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(badge, LV_SIZE_CONTENT, h);
  lv_obj_set_style_radius(badge, h / 2, 0);
  lv_obj_set_style_bg_color(badge, lv_color_hex(info.color), 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(badge, h / 3, 0);
  lv_obj_set_style_pad_column(badge, h / 4, 0);
  lv_obj_set_flex_flow(badge, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(badge, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  console_icon_create(badge, info, h * 6 / 10);
  auto *label = lv_label_create(badge);
  lv_label_set_text(label, info.name);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  if (font) lv_obj_set_style_text_font(label, font, 0);
  return badge;
}

std::string console_art_path(const std::string &root, const ConsoleInfo &info) {
  const std::string path = root + "/boxart/consoles/" + info.key + ".jpg";
  std::error_code ec;
  return std::filesystem::exists(path, ec) ? path : std::string{};
}
