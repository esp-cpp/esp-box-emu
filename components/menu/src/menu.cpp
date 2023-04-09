#include "menu.hpp"

extern "C" {
#include "ui.h"
}

void Menu::init_ui() {
  menu_ui_init();

  // now set up the event callbacks

  // emulation controls
  lv_obj_add_event_cb(ui_resume_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_reset_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_quit_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_load_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_save_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // volume settings
  lv_obj_add_event_cb(ui_volume_inc_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volume_dec_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volume_mute_btn, &Menu::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
}

void Menu::deinit_ui() {
  lv_obj_del(ui_menu_panel);
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
  // volume controls
  bool is_volume_up_button = (target == ui_volume_inc_btn);
  if (is_volume_up_button) {
    return;
  }
  bool is_volume_down_button = (target == ui_volume_dec_btn);
  if (is_volume_down_button) {
    return;
  }
  bool is_mute_button = (target == ui_volume_mute_btn);
  if (is_mute_button) {
    return;
  }
}
