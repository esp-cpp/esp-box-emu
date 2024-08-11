#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "event_manager.hpp"
#include "display.hpp"
#include "high_resolution_timer.hpp"
#include "logger.hpp"

#include "box-emu.hpp"
#include "statistics.hpp"

class Menu {
public:
  static constexpr size_t MAX_SLOT = 5;
  enum class Action { RESUME, RESET, SAVE, LOAD, QUIT };

  typedef std::function<void(Action)> action_fn;
  typedef std::function<std::string()> slot_image_fn;

  struct Config {
    size_t stack_size_bytes = 4 * 1024;
    std::string paused_image_path;
    action_fn action_callback;
    slot_image_fn slot_image_callback;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  explicit Menu(const Config& config)
    : paused_image_path_(config.paused_image_path),
      action_callback_(config.action_callback),
      slot_image_callback_(config.slot_image_callback),
      logger_({.tag="Menu", .level=config.log_level}) {
    init_ui();
    // now start the menu updater task
    task_.periodic(16 * 1000);
    // register events
    using namespace std::placeholders;
    espp::EventManager::get().add_subscriber(mute_button_topic,
                                             "menu",
                                             std::bind(&Menu::on_mute_button_pressed, this, _1),
                                             4 * 1024);
    espp::EventManager::get().add_subscriber(battery_topic,
                                             "menu",
                                             std::bind(&Menu::on_battery, this, _1),
                                             5 * 1024);
    espp::EventManager::get().add_subscriber(volume_changed_topic,
                                             "menu",
                                             std::bind(&Menu::on_volume, this, _1),
                                             4 * 1024);
    logger_.info("Menu created");
  }

  ~Menu() {
    espp::EventManager::get().remove_subscriber(mute_button_topic, "menu");
    espp::EventManager::get().remove_subscriber(battery_topic, "menu");
    espp::EventManager::get().remove_subscriber(volume_changed_topic, "menu");
    task_.stop();
    deinit_ui();
  }

  size_t get_selected_slot() const {
    return selected_slot_;
  }

  void select_slot(size_t slot) {
    selected_slot_ = std::clamp(slot, (size_t)0, MAX_SLOT);
    update_slot_display();
  }

  void next_slot() {
    selected_slot_++;
    // if we go too high, cycle around to 0
    if (selected_slot_ > MAX_SLOT)
      selected_slot_ = 0;
    update_slot_display();
  }

  void previous_slot() {
    selected_slot_--;
    // if we go less than 0, cycle around to the MAX
    if (selected_slot_ < 0)
      selected_slot_ = MAX_SLOT;
    update_slot_display();
  }

  void set_mute(bool muted);

  void toggle_mute() {
    set_mute(!espp::EspBox::get().is_muted());
  }

  void set_audio_level(int new_audio_level);

  void set_brightness(int new_brightness);

  void set_video_setting(VideoSetting setting);

  bool is_paused() const { return paused_; }
  void pause() {
    paused_ = true;
    lv_group_focus_freeze(group_, true);
  }
  void resume() {
    update_shared_state();
    update_slot_display();
    update_pause_image();
    update_fps_label(get_fps());
    paused_ = false;
    lv_group_focus_freeze(group_, false);
  }

protected:
  void init_ui();
  void deinit_ui();
  void update_slot_display();
  void update_slot_label();
  void update_slot_image();
  void update_pause_image();
  void update_fps_label(float fps);

  void update_shared_state() {
    auto &box = espp::EspBox::get();
    set_mute(box.is_muted());
    set_audio_level(box.volume());
    set_brightness(box.brightness());
    set_video_setting(BoxEmu::get().video_setting());
  }

  VideoSetting get_video_setting();

  void on_mute_button_pressed(const std::vector<uint8_t>& data) {
    set_mute(espp::EspBox::get().is_muted());
  }

  void update() {
    if (!paused_) {
      std::lock_guard<std::recursive_mutex> lk(mutex_);
      lv_task_handler();
    }
  }

  static void event_callback(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    auto user_data = lv_event_get_user_data(e);
    auto menu = static_cast<Menu*>(user_data);
    if (!menu) {
      return;
    }
    switch (event_code) {
    case LV_EVENT_SHORT_CLICKED:
      break;
    case LV_EVENT_PRESSED:
      menu->on_pressed(e);
      break;
    case LV_EVENT_VALUE_CHANGED:
      menu->on_value_changed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      menu->on_key(e);
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);
  void on_value_changed(lv_event_t *e);
  void on_key(lv_event_t *e);

  void on_battery(const std::vector<uint8_t>& data);
  void on_volume(const std::vector<uint8_t>& data);

  // LVLG menu objects
  lv_style_t button_style_;
  lv_group_t *group_{nullptr};

  lv_image_dsc_t state_image_;
  lv_image_dsc_t paused_image_;

  std::vector<uint8_t> state_image_data_;
  std::vector<uint8_t> paused_image_data_;

  lv_obj_t *previous_screen_{nullptr};

  int selected_slot_{0};
  std::atomic<bool> paused_{true};
  std::string paused_image_path_;
  action_fn action_callback_;
  slot_image_fn slot_image_callback_;
  espp::HighResolutionTimer task_{{
      .name = "Menu Task",
      .callback = std::bind(&Menu::update, this),
    }};
  espp::Logger logger_;
  std::recursive_mutex mutex_;
};
