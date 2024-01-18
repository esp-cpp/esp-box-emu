#include "gui.hpp"

extern "C" {
#include "ui.h"
#include "ui_helpers.h"
#include "ui_comp.h"
}

void Gui::set_mute(bool muted) {
  hal::set_muted(muted);
  if (muted) {
    lv_obj_add_state(ui_mutebutton, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(ui_mutebutton, LV_STATE_CHECKED);
  }
}

void Gui::set_audio_level(int new_audio_level) {
  new_audio_level = std::clamp(new_audio_level, 0, 100);
  lv_bar_set_value(ui_volumebar, new_audio_level, LV_ANIM_ON);
  hal::set_audio_volume(new_audio_level);
}

void Gui::set_brightness(int new_brightness) {
  new_brightness = std::clamp(new_brightness, 10, 100);
  lv_bar_set_value(ui_brightnessbar, new_brightness, LV_ANIM_ON);
  hal::set_display_brightness((float)new_brightness / 100.0f);
}

void Gui::set_video_setting(VideoSetting setting) {
  hal::set_video_setting(setting);
  lv_dropdown_set_selected(ui_videosettingdropdown, (int)setting);
}

VideoSetting Gui::get_video_setting() {
  return (VideoSetting)(lv_dropdown_get_selected(ui_videosettingdropdown));
}

void Gui::clear_rom_list() {
  // protect since this function is called from another thread context
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // clear the rom list
  for (auto rom : roms_) {
    lv_obj_del(rom);
  }
  roms_.clear();
  rom_infos_.clear();
  focused_rom_ = -1;
}

void Gui::update_rom_list() {
  // protect since this function is called from another thread context
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // // clear the rom list
  clear_rom_list();
  // get the roms from the metadata
  auto roms = parse_metadata("metadata.csv");
  // iterate through the list and get the rom and index for each
  for (const auto& rom : roms) {
    add_rom(rom);
  }
}

void Gui::add_rom(const RomInfo& rom_info) {
  // protect since this function is called from another thread context
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // ensure we don't already have this rom
  if (std::find(rom_infos_.begin(), rom_infos_.end(), rom_info) != rom_infos_.end()) {
    return;
  }
  // make a new rom, which is a button with a label in it
  // make the rom's button
  auto new_rom = lv_btn_create(ui_rompanel);
  lv_obj_set_size(new_rom, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_add_flag(new_rom, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(new_rom, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(new_rom, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(new_rom, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(new_rom, &Gui::event_callback, LV_EVENT_FOCUSED, static_cast<void*>(this));
  lv_obj_center(new_rom);
  // set the rom's label text
  auto label = lv_label_create(new_rom);
  lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_flag(label, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_label_set_text(label, rom_info.name.c_str());
  lv_obj_add_style(label, &rom_label_style_, LV_STATE_DEFAULT);
  lv_obj_center(label);
  // and add it to our vectors
  roms_.push_back(new_rom);
  rom_infos_.push_back(rom_info);
  if (focused_rom_ == -1) {
    // if we don't have a focused rom, then focus this newly added rom!
    on_rom_focused(new_rom);
  }
  // add the rom to the rom screen group
  lv_group_add_obj(rom_screen_group_, new_rom);
}

void Gui::on_rom_focused(lv_obj_t* new_focus) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  if (roms_.size() == 0) {
    return;
  }
  if (new_focus == nullptr) {
    return;
  }
  if (new_focus == roms_[focused_rom_]) {
    // already focused
    return;
  }
  // unfocus all roms
  for (int i=0; i < roms_.size(); i++) {
    auto rom = roms_[i];
    lv_obj_clear_state(rom, LV_STATE_CHECKED);
    if (rom == new_focus && i != focused_rom_) {
      // if the focused_rom variable was not set correctly, set it now.
      focused_rom_ = i;
    }
  }
  // focus
  lv_obj_add_state(new_focus, LV_STATE_CHECKED);
  // lv_obj_scroll_to_view(new_focus, LV_ANIM_ON);
  // update the boxart
  auto boxart_path = rom_infos_[focused_rom_].boxart_path.c_str();
  focused_boxart_ = make_boxart(boxart_path);
  lv_img_set_src(ui_boxart, &focused_boxart_);
}

void Gui::update_haptic_waveform_label() {
  auto haptic_label = fmt::format("{}", haptic_waveform_);
  lv_label_set_text(ui_hapticlabel, haptic_label.c_str());
}

void Gui::deinit_ui() {
  // delete the groups
  lv_group_del(rom_screen_group_);
  lv_group_del(settings_screen_group_);
  // delete the ui
  lv_obj_del(ui_romscreen);
  lv_obj_del(ui_settingsscreen);
}

void Gui::init_ui() {
  // make 2 groups:
  // 1. rom screen
  // 2. settings screen
  rom_screen_group_ = lv_group_create();
  settings_screen_group_ = lv_group_create();

  // get the KEYPAD indev
  auto keypad = get_keypad_input_device();
  if (keypad)
    lv_indev_set_group(keypad, rom_screen_group_);

  ui_init();

  // make the label scrolling animation
  lv_anim_init(&rom_label_animation_template_);
  lv_anim_set_delay(&rom_label_animation_template_, 1000);           /*Wait 1 second to start the first scroll*/
  lv_anim_set_repeat_delay(&rom_label_animation_template_,
                           3000);    /*Repeat the scroll 3 seconds after the label scrolls back to the initial position*/

  /*Initialize the label style with the animation template*/
  lv_style_init(&rom_label_style_);
  lv_style_set_anim(&rom_label_style_, &rom_label_animation_template_);

  lv_obj_set_flex_flow(ui_rompanel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_snap_y(ui_rompanel, LV_SCROLL_SNAP_CENTER);

  lv_bar_set_value(ui_volumebar, hal::get_audio_volume(), LV_ANIM_OFF);

  // rom screen navigation
  lv_obj_add_event_cb(ui_settingsbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_closebutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_playbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // video settings
  lv_obj_add_event_cb(ui_videosettingdropdown, &Gui::event_callback, LV_EVENT_VALUE_CHANGED, static_cast<void*>(this));

  // volume settings
  lv_obj_add_event_cb(ui_volumeupbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volumedownbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_mutebutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // brightness settings
  lv_obj_add_event_cb(ui_brightnessdownbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_brightnessupbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // haptic settings
  lv_obj_add_event_cb(ui_hapticdownbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_hapticupbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_hapticplaybutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // usb button
  lv_obj_add_event_cb(ui_usb_button, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

  // now do the same events for all the same buttons but for the LV_EVENT_KEY
  lv_obj_add_event_cb(ui_settingsbutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_playbutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_videosettingdropdown, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volumeupbutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_volumedownbutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_brightnessdownbutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_brightnessupbutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_mutebutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_hapticdownbutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_hapticupbutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_hapticplaybutton, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));
  lv_obj_add_event_cb(ui_usb_button, &Gui::event_callback, LV_EVENT_KEY, static_cast<void*>(this));

  // ensure the waveform is set and the ui is updated
  set_haptic_waveform(haptic_waveform_);

  // add all the settings buttons to the settings screen group
  lv_group_add_obj(settings_screen_group_, ui_mutebutton);
  lv_group_add_obj(settings_screen_group_, ui_volumedownbutton);
  lv_group_add_obj(settings_screen_group_, ui_volumeupbutton);
  lv_group_add_obj(settings_screen_group_, ui_brightnessdownbutton);
  lv_group_add_obj(settings_screen_group_, ui_brightnessupbutton);
  lv_group_add_obj(settings_screen_group_, ui_videosettingdropdown);
  lv_group_add_obj(settings_screen_group_, ui_hapticdownbutton);
  lv_group_add_obj(settings_screen_group_, ui_hapticupbutton);
  lv_group_add_obj(settings_screen_group_, ui_hapticplaybutton);
  lv_group_add_obj(settings_screen_group_, ui_usb_button);

  // set the focused style for all the buttons to have a red border
  lv_style_init(&button_style_);
  lv_style_set_border_color(&button_style_, lv_palette_main(LV_PALETTE_RED));
  lv_style_set_border_width(&button_style_, 2);

  lv_obj_add_style(ui_settingsbutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_playbutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_closebutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_volumeupbutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_volumedownbutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_brightnessdownbutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_brightnessupbutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_mutebutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_hapticupbutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_hapticdownbutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_hapticplaybutton, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_videosettingdropdown, &button_style_, LV_STATE_FOCUSED);
  lv_obj_add_style(ui_usb_button, &button_style_, LV_STATE_FOCUSED);

  update_rom_list();

  focus_rommenu();
}

void Gui::load_rom_screen() {
  logger_.info("Loading rom screen");
  _ui_screen_change( &ui_romscreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, &ui_settingsscreen_screen_init);
  // update the rom list
  update_rom_list();
  focus_rommenu();
}

void Gui::load_settings_screen() {
  logger_.info("Loading settings screen");
  _ui_screen_change( &ui_settingsscreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, &ui_romscreen_screen_init);
  focus_settings();
}

void Gui::on_value_changed(lv_event_t *e) {
  lv_obj_t * target = lv_event_get_target(e);
  logger_.info("Value changed: {}", fmt::ptr(target));
  // is it the settings button?
  bool is_video_setting = (target == ui_videosettingdropdown);
  if (is_video_setting) {
    set_video_setting(this->get_video_setting());
    return;
  }
}

void Gui::on_pressed(lv_event_t *e) {
  lv_obj_t * target = lv_event_get_target(e);
  logger_.info("PRESSED: {}", fmt::ptr(target));
  // is it the settings button?
  bool is_settings_button = (target == ui_settingsbutton);
  if (is_settings_button) {
    // set the settings screen group as the default group
    focus_settings();
    return;
  }
  // volume controls
  bool is_volume_up_button = (target == ui_volumeupbutton);
  if (is_volume_up_button) {
    set_audio_level(hal::get_audio_volume() + 10);
    return;
  }
  bool is_volume_down_button = (target == ui_volumedownbutton);
  if (is_volume_down_button) {
    set_audio_level(hal::get_audio_volume() - 10);
    return;
  }
  bool is_mute_button = (target == ui_mutebutton);
  if (is_mute_button) {
    toggle_mute();
    return;
  }
  // brightness controlsn
  bool is_brightness_up_button = (target == ui_brightnessupbutton);
  if (is_brightness_up_button) {
    int brightness = hal::get_display_brightness() * 100.0f;
    set_brightness(brightness + 10);
    return;
  }
  bool is_brightness_down_button = (target == ui_brightnessdownbutton);
  if (is_brightness_down_button) {
    int brightness = hal::get_display_brightness() * 100.0f;
    set_brightness(brightness - 10);
    return;
  }
  // haptic controls
  bool is_haptic_up_button = (target == ui_hapticupbutton);
  if (is_haptic_up_button) {
    next_haptic_waveform();
    return;
  }
  bool is_haptic_down_button = (target == ui_hapticdownbutton);
  if (is_haptic_down_button) {
    previous_haptic_waveform();
    return;
  }
  bool is_hapticplay_button = (target == ui_hapticplaybutton);
  if (is_hapticplay_button) {
    play_haptic_();
    return;
  }
  // or is it the play button?
  bool is_play_button = (target == ui_playbutton);
  if (is_play_button) {
    ready_to_play_ = true;
    freeze_focus();
    return;
  }
  bool is_close_button = (target == ui_closebutton);
  if (is_close_button) {
    focus_rommenu();
    return;
  }
  bool is_usb_button = (target == ui_usb_button);
  if (is_usb_button) {
    toggle_usb();
    return;
  }
  // or is it one of the roms?
  if (std::find(roms_.begin(), roms_.end(), target) != roms_.end()) {
    // it's one of the roms, focus it! this was pressed, so don't scroll (it
    // will already scroll)
    on_rom_focused(target);
  }
}

void Gui::on_volume(const std::vector<uint8_t>& data) {
  // the volume was changed, update our display of the volume
  lv_bar_set_value(ui_volumebar, hal::get_audio_volume(), LV_ANIM_ON);
}

void Gui::on_battery(const std::vector<uint8_t>& data) {
  // parse the data as a BatteryInfo message
  std::error_code ec;
  auto battery_info = espp::deserialize<BatteryInfo>(data, ec);
  if (ec) {
    return;
  }
  // update the battery soc labels (text)
  lv_label_set_text(ui_battery_soc_text, fmt::format("{} %", (int)battery_info.level).c_str());
  lv_label_set_text(ui_battery_soc_text_1, fmt::format("{} %", (int)battery_info.level).c_str());
  // update the battery soc symbols (battery icon using LVGL font symbols)
  if (battery_info.level > 90.0f) {
    lv_label_set_text(ui_battery_soc_symbol, LV_SYMBOL_BATTERY_FULL);
    lv_label_set_text(ui_battery_soc_symbol_1, LV_SYMBOL_BATTERY_FULL);
  } else if (battery_info.level > 70.0f) {
    lv_label_set_text(ui_battery_soc_symbol, LV_SYMBOL_BATTERY_3);
    lv_label_set_text(ui_battery_soc_symbol_1, LV_SYMBOL_BATTERY_3);
  } else if (battery_info.level > 50.0f) {
    lv_label_set_text(ui_battery_soc_symbol, LV_SYMBOL_BATTERY_2);
    lv_label_set_text(ui_battery_soc_symbol_1, LV_SYMBOL_BATTERY_2);
  } else if (battery_info.level > 30.0f) {
    lv_label_set_text(ui_battery_soc_symbol, LV_SYMBOL_BATTERY_1);
    lv_label_set_text(ui_battery_soc_symbol_1, LV_SYMBOL_BATTERY_1);
  } else {
    lv_label_set_text(ui_battery_soc_symbol, LV_SYMBOL_BATTERY_EMPTY);
    lv_label_set_text(ui_battery_soc_symbol_1, LV_SYMBOL_BATTERY_EMPTY);
  }
  // if the battery is charging, then show the charging symbol
  if (battery_info.charge_rate > 0.0f) {
    lv_label_set_text(ui_battery_charging_symbol, LV_SYMBOL_CHARGE);
    lv_label_set_text(ui_battery_charging_symbol_1, LV_SYMBOL_CHARGE);
  } else {
    lv_label_set_text(ui_battery_charging_symbol, "");
    lv_label_set_text(ui_battery_charging_symbol_1, "");
  }
}

void Gui::toggle_usb() {
  fmt::print("Toggling USB\n");
  // toggle the usb
  if (usb_is_enabled()) {
    usb_deinit();
  } else {
    usb_init();
  }
  // update the label
  if (usb_is_enabled()) {
    lv_label_set_text(ui_usb_label, "Enabled");
  } else {
    lv_label_set_text(ui_usb_label, "Disabled");
  }
}

void Gui::freeze_focus() {
  logger_.debug("Freezing focus");
  // freeze the focus
  lv_group_focus_freeze(rom_screen_group_, true);
  lv_group_focus_freeze(settings_screen_group_, true);
  // set editing false for the settings screen group
  lv_group_set_editing(settings_screen_group_, false);
}

void Gui::focus_rommenu() {
  freeze_focus();
  // focus the rom screen group
  logger_.debug("Focusing rom screen group");
  lv_group_focus_freeze(rom_screen_group_, false);
  auto keypad = get_keypad_input_device();
  if (keypad)
    lv_indev_set_group(keypad, rom_screen_group_);
}

void Gui::focus_settings() {
  freeze_focus();
  // focus the rom screen group
  logger_.debug("Focusing settings screen group");
  lv_group_focus_freeze(settings_screen_group_, false);
  // NOTE: we don't set editing here since we use it to manage the dropdown
  auto keypad = get_keypad_input_device();
  if (keypad)
    lv_indev_set_group(keypad, settings_screen_group_);
}

void Gui::on_key(lv_event_t *e) {
  // print the key
  auto key = lv_indev_get_key(lv_indev_get_act());
  // get which screen is currently loaded
  auto current_screen = lv_scr_act();
  bool is_rom_screen = (current_screen == ui_romscreen);
  bool is_settings_screen = (current_screen == ui_settingsscreen);
  bool is_settings_edit = lv_group_get_editing(settings_screen_group_);

  // see if the target is the videosettingdropdown
  lv_obj_t * target = lv_event_get_target(e);
  // TODO: this is a really hacky way of getting the dropdown to work within a
  // group when managed by the keypad input device. I'm not sure if there's a
  // better way to do this, but this works for now.
  bool is_video_setting = (target == ui_videosettingdropdown);
  if (key == LV_KEY_ESC) {
    // if we're in the settings screen group, then go back to the rom screen
    if (is_settings_screen) {
      if (!is_settings_edit) {
        load_rom_screen();
      } else {
        // otherwise, close the dropdown
        lv_dropdown_close(ui_videosettingdropdown);
        lv_group_set_editing(settings_screen_group_, false);
      }
    } else if (is_rom_screen) {
      load_settings_screen();
    }
  } else if (key == LV_KEY_ENTER) {
    if (is_rom_screen) {
      // play the focused rom
      ready_to_play_ = true;
    } else if (is_settings_screen) {
      // handle some specific things we need to do for the dropdown -.-
      // They say that in v9 they will have a better way to do this...
      if (is_video_setting) {
        if (is_settings_edit) {
          // lv_dropdown_close(ui_videosettingdropdown);
          lv_group_set_editing(settings_screen_group_, false);
        } else {
          // lv_dropdown_open(ui_videosettingdropdown);
          lv_group_set_editing(settings_screen_group_, true);
        }
      }
    }
  } else if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) {
    if (is_settings_screen) {
      if (!is_settings_edit) {
        // if we're in the settings screen group, then focus the next item
        lv_group_focus_next(settings_screen_group_);
      }
    } else if (is_rom_screen) {
      // focus the next rom
      lv_group_focus_next(rom_screen_group_);
    }
  } else if (key == LV_KEY_LEFT || key == LV_KEY_UP) {
    if (is_settings_screen) {
      if (!is_settings_edit) {
        // if we're in the settings screen group, then focus the next item
        lv_group_focus_prev(settings_screen_group_);
      }
    } else if (is_rom_screen) {
      // focus the next rom
      lv_group_focus_prev(rom_screen_group_);
    }
  }
}
