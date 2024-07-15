#include "menu.hpp"
#include <fstream>

extern "C" {
#include "ui.h"
}

void Menu::init_ui() {
  logger_.info("Loading UI");
  // save the previous screen to return to it when we destroy ourselves.
  previous_screen_ = lv_scr_act();

  // create the default group
  group_ = lv_group_create();
  lv_group_set_default(group_);

  // get the KEYPAD indev
  auto keypad = BoxEmu::get().keypad();
  if (keypad) {
    auto input = keypad->get_input_device();
    if (input)
      lv_indev_set_group(input, group_);
  }

  // now initialize our UI
  menu_ui_init();

  // now set up the event callbacks

  // emulation controls
  lv_obj_add_event_cb(ui_resume_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_reset_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_quit_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_load_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_save_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // save state slot
  lv_obj_add_event_cb(ui_btn_slot_dec, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_btn_slot_inc, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // volume settings
  lv_obj_add_event_cb(ui_volume_inc_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volume_dec_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volume_mute_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // brightness settings
  lv_obj_add_event_cb(ui_brightness_inc_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_brightness_dec_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // // now do all the same buttons but with the LV_EVENT_KEY event
  lv_obj_add_event_cb(ui_resume_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_reset_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_quit_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_load_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_save_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_btn_slot_dec, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_btn_slot_inc, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volume_inc_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volume_dec_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volume_mute_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_brightness_inc_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_brightness_dec_btn, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));

  lv_obj_add_event_cb(ui_Dropdown2, &Menu::event_callback, LV_EVENT_KEY, static_cast<void*>(this));

  // video settings
  lv_obj_add_event_cb(ui_Dropdown2, &Menu::event_callback, LV_EVENT_VALUE_CHANGED, static_cast<void*>(this));

  // // add all the buttons to the group
  lv_group_add_obj(group_, ui_resume_btn);
  lv_group_add_obj(group_, ui_volume_mute_btn);
  lv_group_add_obj(group_, ui_volume_dec_btn);
  lv_group_add_obj(group_, ui_volume_inc_btn);
  lv_group_add_obj(group_, ui_brightness_dec_btn);
  lv_group_add_obj(group_, ui_brightness_inc_btn);
  lv_group_add_obj(group_, ui_btn_slot_dec);
  lv_group_add_obj(group_, ui_btn_slot_inc);
  lv_group_add_obj(group_, ui_load_btn);
  lv_group_add_obj(group_, ui_save_btn);
  lv_group_add_obj(group_, ui_Dropdown2);
  lv_group_add_obj(group_, ui_reset_btn);
  lv_group_add_obj(group_, ui_quit_btn);

  // set the focused style for all the buttons to have a red border
  lv_style_init(&button_style_);
  lv_style_set_border_color(&button_style_, lv_palette_main(LV_PALETTE_RED));
  lv_style_set_border_width(&button_style_, 2);

  lv_obj_add_style(ui_resume_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_volume_mute_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_volume_dec_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_volume_inc_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_brightness_dec_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_brightness_inc_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_btn_slot_dec, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_btn_slot_inc, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_load_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_save_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_Dropdown2, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_reset_btn, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_quit_btn, &button_style_, LV_STATE_FOCUSED);

  // now focus the resume button
  lv_group_focus_obj(ui_resume_btn);
  lv_group_focus_freeze(group_, false);

  update_fps_label(get_fps());
}

void Menu::deinit_ui() {
  lv_scr_load(previous_screen_);
  lv_obj_del(ui_Screen1);
  // delete the group
  lv_group_del(group_);
}

void Menu::update_slot_display() {
  update_slot_label();
  update_slot_image();
}

void Menu::update_slot_label() {
  auto slot = fmt::format("Save Slot {}", selected_slot_);
  lv_label_set_text(ui_slot_label, slot.c_str());
}

void Menu::update_slot_image() {
  // clear the image in case we don't have a new one to set
  lv_img_set_src(ui_slot_image, nullptr);
  if (slot_image_callback_) {
    auto image = slot_image_callback_();
    logger_.info("Updating slot image to '{}'", image);
    if (image.empty()) {
      logger_.info("No slot image to display");
      return;
    }
    // load the image data
    std::ifstream file(image, std::ios::binary);
    if (!file) {
      logger_.error("Failed to open image file {}", image);
      return;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    uint16_t width, height;
    static constexpr int header_size = 4;
    uint8_t header[header_size];
    file.read((char*)header, header_size);
    width = (header[0] << 8) | (header[1]);
    height = (header[2] << 8) | (header[3]);
    logger_.info("Slot image is {}x{}", width, height);
    int state_image_data_size = size - header_size;
    state_image_data_.resize(state_image_data_size);
    file.read((char*)state_image_data_.data(), state_image_data_size);
    file.close();
    state_image_.header.cf = LV_IMG_CF_TRUE_COLOR;
    state_image_.header.always_zero = 0;
    state_image_.header.reserved = 0;
    state_image_.header.w = width;
    state_image_.header.h = height;
    state_image_.data_size = width * height * 2;
    state_image_.data = (const uint8_t*)state_image_data_.data();
    lv_img_set_src(ui_slot_image, &state_image_);
    lv_img_set_size_mode(ui_slot_image, LV_IMG_SIZE_MODE_REAL);
    // set the scaling so that the image fits in the slot
    auto scale = std::min(80.0f / width, 60.0f / height);
    logger_.info("Scaling image by {}", scale);
    lv_img_set_zoom(ui_slot_image, (uint16_t)(scale * 256.));
    lv_obj_set_size(ui_slot_image, width * scale, height * scale);
  } else {
    logger_.warn("No slot image callback set");
  }
}

void Menu::update_pause_image() {
  // clear the image in case we don't have a new one to set
  lv_img_set_src(ui_pause_image, nullptr);
  logger_.info("Updating pause image to '{}'", paused_image_path_);
  // load the image data
  std::ifstream file(paused_image_path_, std::ios::binary);
  if (!file) {
    logger_.error("Failed to open image file {}", paused_image_path_);
    return;
  }
  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);
  uint16_t width, height;
  static constexpr int header_size = 4;
  uint8_t header[header_size];
  file.read((char*)header, header_size);
  width = (header[0] << 8) | (header[1]);
  height = (header[2] << 8) | (header[3]);
  logger_.info("Paused image is {}x{}", width, height);
  int paused_image_data_size = size - header_size;
  paused_image_data_.resize(paused_image_data_size);
  file.read((char*)paused_image_data_.data(), paused_image_data_size);
  file.close();
  paused_image_.header.cf = LV_IMG_CF_TRUE_COLOR;
  paused_image_.header.always_zero = 0;
  paused_image_.header.reserved = 0;
  paused_image_.header.w = width;
  paused_image_.header.h = height;
  paused_image_.data_size = width * height * 2;
  paused_image_.data = (const uint8_t*)paused_image_data_.data();
  lv_img_set_src(ui_pause_image, &paused_image_);
  lv_img_set_size_mode(ui_pause_image, LV_IMG_SIZE_MODE_REAL);
  // lv_img_set_size(ui_pause_image, width, height);
  // set the scaling so that the image fits in the slot
  auto scale = std::min(80.0f / width, 60.0f / height);
  logger_.info("Setting pause image scale to {}", scale);
  lv_img_set_zoom(ui_pause_image, uint16_t(scale * 256.));
  lv_obj_set_size(ui_pause_image, width * scale, height * scale);
}

void Menu::update_fps_label(float fps) {
  auto label = fmt::format("{:0.1f} FPS", fps);
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  lv_label_set_text(ui_fps_label, label.c_str());
}

void Menu::set_mute(bool muted) {
  espp::EspBox::get().mute(muted);
  if (muted) {
    lv_obj_add_state(ui_volume_mute_btn, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(ui_volume_mute_btn, LV_STATE_CHECKED);
  }
}

void Menu::set_audio_level(int new_audio_level) {
  new_audio_level = std::clamp(new_audio_level, 0, 100);
  lv_bar_set_value(ui_Bar2, new_audio_level, LV_ANIM_ON);
  espp::EspBox::get().volume(new_audio_level);
}

void Menu::set_brightness(int new_brightness) {
  new_brightness = std::clamp(new_brightness, 10, 100);
  lv_bar_set_value(ui_brightness_bar, new_brightness, LV_ANIM_ON);
  espp::EspBox::get().brightness((float)new_brightness);
}

void Menu::set_video_setting(VideoSetting setting) {
  BoxEmu::get().video_setting(setting);
  lv_dropdown_set_selected(ui_Dropdown2, (int)setting);
}

VideoSetting Menu::get_video_setting() {
  return (VideoSetting)(lv_dropdown_get_selected(ui_Dropdown2));
}

void Menu::on_value_changed(lv_event_t *e) {
  lv_obj_t * target = lv_event_get_target(e);
  logger_.info("Value changed: {}", fmt::ptr(target));
  // is it the settings button?
  bool is_video_setting = (target == ui_Dropdown2);
  if (is_video_setting) {
    set_video_setting(this->get_video_setting());
    return;
  }
}

void Menu::on_pressed(lv_event_t *e) {
  lv_obj_t * target = lv_event_get_target(e);
  logger_.info("PRESSED: {}", fmt::ptr(target));
  // emulation controls
  bool is_resume = (target == ui_resume_btn);
  if (is_resume) {
    action_callback_(Action::RESUME);
    return;
  }
  bool is_reset = (target == ui_reset_btn);
  if (is_reset) {
    action_callback_(Action::RESET);
    return;
  }
  bool is_quit = (target == ui_quit_btn);
  if (is_quit) {
    action_callback_(Action::QUIT);
    return;
  }
  bool is_save = (target == ui_save_btn);
  if (is_save) {
    action_callback_(Action::SAVE);
    // when we save, the paused state is now the saved state
    update_slot_display();
    return;
  }
  bool is_load = (target == ui_load_btn);
  if (is_load) {
    action_callback_(Action::LOAD);
    // when we load, the paused state is now the loaded state
    update_pause_image();
    return;
  }
  // slot controls
  bool is_slot_up = (target == ui_btn_slot_inc);
  if (is_slot_up) {
    next_slot();
    update_slot_display();
    return;
  }
  bool is_slot_down = (target == ui_btn_slot_dec);
  if (is_slot_down) {
    previous_slot();
    update_slot_display();
    return;
  }
  // volume controls
  bool is_volume_up_button = (target == ui_volume_inc_btn);
  if (is_volume_up_button) {
    set_audio_level(espp::EspBox::get().volume() + 10);
    return;
  }
  bool is_volume_down_button = (target == ui_volume_dec_btn);
  if (is_volume_down_button) {
    set_audio_level(espp::EspBox::get().volume() - 10);
    return;
  }
  bool is_mute_button = (target == ui_volume_mute_btn);
  if (is_mute_button) {
    toggle_mute();
    return;
  }
  // brightness controls
  bool is_brightness_up_button = (target == ui_brightness_inc_btn);
  if (is_brightness_up_button) {
    set_brightness(espp::EspBox::get().brightness() + 10);
    return;
  }
  bool is_brightness_down_button = (target == ui_brightness_dec_btn);
  if (is_brightness_down_button) {
    set_brightness(espp::EspBox::get().brightness() - 10);
    return;
  }
}

void Menu::on_volume(const std::vector<uint8_t>& data) {
  // the volume was changed, update our display of the volume
  lv_bar_set_value(ui_Bar2, espp::EspBox::get().volume(), LV_ANIM_ON);
}

void Menu::on_battery(const std::vector<uint8_t>& data) {
  // parse the data as a BatteryInfo message
  std::error_code ec;
  auto battery_info = espp::deserialize<BatteryInfo>(data, ec);
  if (ec) {
    return;
  }
  // update the battery soc labels (text)
  lv_label_set_text(ui_menu_battery_soc_text, fmt::format("{} %", (int)battery_info.level).c_str());
  // update the battery soc symbols (battery icon using LVGL font symbols)
  if (battery_info.level > 90.0f) {
    lv_label_set_text(ui_menu_battery_soc_symbol, LV_SYMBOL_BATTERY_FULL);
  } else if (battery_info.level > 70.0f) {
    lv_label_set_text(ui_menu_battery_soc_symbol, LV_SYMBOL_BATTERY_3);
  } else if (battery_info.level > 50.0f) {
    lv_label_set_text(ui_menu_battery_soc_symbol, LV_SYMBOL_BATTERY_2);
  } else if (battery_info.level > 30.0f) {
    lv_label_set_text(ui_menu_battery_soc_symbol, LV_SYMBOL_BATTERY_1);
  } else {
    lv_label_set_text(ui_menu_battery_soc_symbol, LV_SYMBOL_BATTERY_EMPTY);
  }
  // if the battery is charging, then show the charging symbol
  if (battery_info.charge_rate > 0.0f) {
    lv_label_set_text(ui_menu_battery_charging_symbol, LV_SYMBOL_CHARGE);
  } else {
    lv_label_set_text(ui_menu_battery_charging_symbol, "");
  }
}

void Menu::on_key(lv_event_t *e) {
  // get the target of the event
  lv_obj_t * target = lv_event_get_target(e);
  // determine if this is the dropdown and, if so if it is open
  // TODO: this is a really hacky way of getting the dropdown to work within a
  // group when managed by the keypad input device. I'm not sure if there's a
  // better way to do this, but this works for now.
  bool is_video_setting = (target == ui_Dropdown2);
  bool is_video_edit = lv_group_get_editing(group_);
  auto key = lv_indev_get_key(lv_indev_get_act());
  // now handle the keys
  if (key == LV_KEY_ESC) {
    if (!is_video_edit) {
      // if we're editing the dropdown, then close the menu
      action_callback_(Action::RESUME);
    } else {
      // otherwise, close the dropdown
      lv_dropdown_close(ui_Dropdown2);
      lv_group_set_editing(group_, false);
    }
  } else if (key == LV_KEY_ENTER) {
    // handle some specific things we need to do for the dropdown -.-
    // They say that in v9 they will have a better way to do this...
    if (is_video_setting) {
      if (is_video_edit) {
        // lv_dropdown_close(ui_videosettingdropdown);
        lv_group_set_editing(group_, false);
      } else {
        // lv_dropdown_open(ui_videosettingdropdown);
        lv_group_set_editing(group_, true);
      }
    }
  } else if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) {
    if (!is_video_edit) {
      // if we're in the settings screen group, then focus the next item
      lv_group_focus_next(group_);
    }
  } else if (key == LV_KEY_LEFT || key == LV_KEY_UP) {
    if (!is_video_edit) {
      // if we're in the settings screen group, then focus the next item
      lv_group_focus_prev(group_);
    }
  }

}
