#include "menu.hpp"

extern "C" {
#include "ui.h"
}

void Menu::init_ui() {
  // save the previous screen to return to it when we destroy ourselves.
  previous_screen_ = lv_scr_act();

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

  // video settings
  lv_obj_add_event_cb(ui_Dropdown2, &Menu::event_callback, LV_EVENT_VALUE_CHANGED, static_cast<void*>(this));
}

void Menu::deinit_ui() {
  lv_scr_load(previous_screen_);
  lv_obj_del(ui_Screen1);
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

}

void Menu::set_mute(bool muted) {
  set_muted(muted);
  if (muted) {
    lv_obj_add_state(ui_volume_mute_btn, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(ui_volume_mute_btn, LV_STATE_CHECKED);
  }
}

void Menu::set_audio_level(int new_audio_level) {
  new_audio_level = std::clamp(new_audio_level, 0, 100);
  lv_bar_set_value(ui_Bar2, new_audio_level, LV_ANIM_ON);
  set_audio_volume(new_audio_level);
}

void Menu::set_video_setting(VideoSetting setting) {
  ::set_video_setting(setting);
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
    return;
  }
  bool is_load = (target == ui_load_btn);
  if (is_load) {
    action_callback_(Action::LOAD);
    return;
  }
  // slot controls
  bool is_slot_up = (target == ui_btn_slot_inc);
  if (is_slot_up) {
    next_slot();
    return;
  }
  bool is_slot_down = (target == ui_btn_slot_dec);
  if (is_slot_down) {
    previous_slot();
    return;
  }
  // volume controls
  bool is_volume_up_button = (target == ui_volume_inc_btn);
  if (is_volume_up_button) {
    set_audio_level(get_audio_volume() + 10);
    return;
  }
  bool is_volume_down_button = (target == ui_volume_dec_btn);
  if (is_volume_down_button) {
    set_audio_level(get_audio_volume() - 10);
    return;
  }
  bool is_mute_button = (target == ui_volume_mute_btn);
  if (is_mute_button) {
    toggle_mute();
    return;
  }
}
