#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "event_manager.hpp"
#include "display.hpp"
#include "jpeg.hpp"
#include "task.hpp"
#include "logger.hpp"

#include "battery.hpp"
#include "fs_init.hpp"
#include "input.h"
#include "hal_events.hpp"
#include "i2s_audio.h"
#include "rom_info.hpp"
#include "spi_lcd.h"
#include "video_setting.hpp"
#include "usb.hpp"

class Gui {
public:
  typedef std::function<void(void)> play_haptic_fn;
  typedef std::function<void(int)> set_waveform_fn;
  typedef std::function<void(int, int)> set_haptic_slot_fn;

  struct Config {
    play_haptic_fn play_haptic;
    set_waveform_fn set_waveform;
    std::shared_ptr<espp::Display> display;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  explicit Gui(const Config& config)
    : play_haptic_(config.play_haptic),
      set_waveform_(config.set_waveform),
      display_(config.display),
      logger_({.tag="Gui", .level=config.log_level}) {
    init_ui();
    update_shared_state();
    // now start the gui updater task
    using namespace std::placeholders;
    task_ = espp::Task::make_unique({
        .name = "Gui Task",
        .callback = std::bind(&Gui::update, this, _1, _2),
        .stack_size_bytes = 6 * 1024
      });
    task_->start();
    // register events
    espp::EventManager::get().add_subscriber(mute_button_topic,
                                             "gui",
                                             std::bind(&Gui::on_mute_button_pressed, this, _1));
    espp::EventManager::get().add_subscriber(battery_topic,
                                             "gui",
                                             std::bind(&Gui::on_battery, this, _1));
  }

  ~Gui() {
    espp::EventManager::get().remove_subscriber(mute_button_topic, "gui");
    espp::EventManager::get().remove_subscriber(battery_topic, "gui");
    task_->stop();
    deinit_ui();
  }

  void ready_to_play(bool new_state) {
    ready_to_play_ = new_state;
  }

  bool ready_to_play() const {
    return ready_to_play_;
  }

  void set_mute(bool muted);

  void toggle_mute() {
    set_mute(!is_muted());
  }

  void set_audio_level(int new_audio_level);

  void set_brightness(int new_brightness);

  void set_video_setting(VideoSetting setting);

  void clear_rom_list();

  void add_rom(const RomInfo& rom);

  std::optional<const RomInfo*> get_selected_rom() const {
    if (focused_rom_ < 0 || focused_rom_ >= rom_infos_.size()) {
      return std::nullopt;
    }
    return &rom_infos_[focused_rom_];
  }

  void pause() {
    paused_ = true;
    freeze_focus();
  }
  void resume() {
    update_shared_state();
    paused_ = false;
    focus_rommenu();
  }

  void set_haptic_waveform(int new_waveform) {
    if (new_waveform > 123) {
      new_waveform = 1;
    } else if (new_waveform <= 0) {
      new_waveform = 123;
    }
    haptic_waveform_ = new_waveform;
    set_waveform_(haptic_waveform_);
    update_haptic_waveform_label();
  }

  void next_haptic_waveform() {
    set_haptic_waveform(haptic_waveform_ + 1);
  }

  void previous_haptic_waveform() {
    set_haptic_waveform(haptic_waveform_ - 1);
  }

  void update_haptic_waveform_label();

  void update_rom_list();

protected:
  void init_ui();
  void deinit_ui();

  void freeze_focus();
  void focus_rommenu();
  void focus_settings();

  void load_rom_screen();
  void load_settings_screen();

  void toggle_usb();

  void update_shared_state() {
    set_mute(is_muted());
    set_audio_level(get_audio_volume());
    set_brightness(get_display_brightness() * 100.0f);
    set_video_setting(::get_video_setting());
  }

  VideoSetting get_video_setting();

  void on_rom_focused(lv_obj_t *new_focus);

  void on_mute_button_pressed(const std::vector<uint8_t>& data) {
    set_mute(is_muted());
  }

  void on_battery(const std::vector<uint8_t>& data);

  lv_img_dsc_t make_boxart(const std::string& path) {
    // load the file
    // auto start = std::chrono::high_resolution_clock::now();
    decoder_.decode(path.c_str());
    // auto end = std::chrono::high_resolution_clock::now();
    // auto elapsed = std::chrono::duration<float>(end-start).count();
    // fmt::print("Decoding took {:.3f}s\n", elapsed);
    // make the descriptor
    lv_img_dsc_t img_desc = {
      .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = (uint32_t)decoder_.get_width(),
        .h = (uint32_t)decoder_.get_height(),
      },
      .data_size = (uint32_t)decoder_.get_size(),
      .data = decoder_.get_decoded_data(),
    };
    // and return it
    return img_desc;
  }

  bool update(std::mutex& m, std::condition_variable& cv) {
    if (!paused_) {
      std::lock_guard<std::recursive_mutex> lk(mutex_);
      lv_task_handler();
    }
    {
      using namespace std::chrono_literals;
      std::unique_lock<std::mutex> lk(m);
      cv.wait_for(lk, 16ms);
    }
    // don't want to stop the task
    return false;
  }

  static void event_callback(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    auto user_data = lv_event_get_user_data(e);
    auto gui = static_cast<Gui*>(user_data);
    if (!gui) {
      return;
    }
    switch (event_code) {
    case LV_EVENT_SHORT_CLICKED:
      break;
    case LV_EVENT_PRESSED:
    case LV_EVENT_CLICKED:
      gui->on_pressed(e);
      break;
    case LV_EVENT_VALUE_CHANGED:
      gui->on_value_changed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      gui->on_key(e);
      break;
    case LV_EVENT_FOCUSED:
      gui->on_rom_focused(lv_event_get_target(e));
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);
  void on_value_changed(lv_event_t *e);
  void on_key(lv_event_t *e);

  // LVLG gui objects
  std::vector<RomInfo> rom_infos_;
  std::vector<lv_obj_t*> roms_;
  std::atomic<int> focused_rom_{-1};
  lv_img_dsc_t focused_boxart_;

  // style for buttons
  lv_style_t button_style_;

  lv_anim_t rom_label_animation_template_;
  lv_style_t rom_label_style_;

  lv_group_t *rom_screen_group_;
  lv_group_t *settings_screen_group_;

  Jpeg decoder_;

  play_haptic_fn play_haptic_;
  set_waveform_fn set_waveform_;
  std::atomic<int> haptic_waveform_{12};

  std::atomic<bool> ready_to_play_{false};
  std::atomic<bool> paused_{false};
  std::shared_ptr<espp::Display> display_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::recursive_mutex mutex_;
};
