#pragma once

#include <memory>
#include <mutex>
#include <vector>

extern "C" {
#include "ui.h"
#include "ui_comp.h"
}

#include "display.hpp"
#include "jpeg.hpp"
#include "task.hpp"
#include "logger.hpp"

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

  enum class VideoSetting { ORIGINAL, FIT, FILL, MAX_UNUSED };

  Gui(const Config& config)
    : play_haptic_(config.play_haptic),
      set_waveform_(config.set_waveform),
      display_(config.display),
      logger_({.tag="Gui", .level=config.log_level}) {
    init_ui();
    // now start the gui updater task
    using namespace std::placeholders;
    task_ = espp::Task::make_unique({
        .name = "Gui Task",
        .callback = std::bind(&Gui::update, this, _1, _2),
        .stack_size_bytes = 6 * 1024
      });
    task_->start();
  }

  void ready_to_play(bool new_state) {
    ready_to_play_ = new_state;
  }

  bool ready_to_play() {
    return ready_to_play_;
  }

  void set_mute(bool muted) {
    muted_ = muted;
    if (muted_) {
      lv_obj_add_state(ui_mutebutton, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(ui_mutebutton, LV_STATE_CHECKED);
    }
  }

  void toggle_mute() {
    set_mute(!muted_);
  }

  void set_audio_level(int new_audio_level) {
    audio_level_ = std::clamp(new_audio_level, 0, 100);
    lv_bar_set_value(ui_volumebar, audio_level_, LV_ANIM_ON);
  }

  int get_audio_level() {
    if (muted_) return 0;
    return audio_level_;
  }

  void next_video_setting() {
    int current_option = lv_dropdown_get_selected(ui_videosettingdropdown);
    int max_options = lv_dropdown_get_option_cnt(ui_videosettingdropdown);
    if (current_option < (max_options-1)) {
      current_option++;
    } else {
      current_option = 0;
    }
    lv_dropdown_set_selected(ui_videosettingdropdown, current_option);
  }

  void prev_video_setting() {
    int current_option = lv_dropdown_get_selected(ui_videosettingdropdown);
    int max_options = lv_dropdown_get_option_cnt(ui_videosettingdropdown);
    if (current_option > 0) {
      current_option = max_options - 1;
    } else {
      current_option--;
    }
    lv_dropdown_set_selected(ui_videosettingdropdown, current_option);
  }

  void set_video_setting(VideoSetting setting) {
    lv_dropdown_set_selected(ui_videosettingdropdown, (int)setting);
  }

  VideoSetting get_video_setting() {
    return (VideoSetting)(lv_dropdown_get_selected(ui_videosettingdropdown));
  }

  void add_rom(const std::string& name, const std::string& image_path) {
    // protect since this function is called from another thread context
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    // make a new rom, which is a button with a label in it
    // make the rom's button
    auto new_rom = lv_btn_create(ui_rompanel);
    lv_obj_set_size(new_rom, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_flag( new_rom, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag( new_rom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(new_rom, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
    lv_obj_center(new_rom);
    // set the rom's label text
    auto label = lv_label_create(new_rom);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(label, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_label_set_text(label, name.c_str());
    lv_obj_add_style(label, &rom_label_style_, LV_STATE_DEFAULT);
    lv_obj_center(label);
    // and add it to our vector
    roms_.push_back(new_rom);
    boxart_paths_.push_back(image_path);
    if (focused_rom_ == -1) {
      // if we don't have a focused rom, then focus this newly added rom!
      focus_rom(new_rom);
    }
  }

  size_t get_selected_rom_index() {
    return focused_rom_;
  }

  void pause() { paused_ = true; }
  void resume() { paused_ = false; }

  void next() {
    // protect since this function is called from another thread context
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if (roms_.size() == 0) {
      return;
    }
    // focus the next rom
    focused_rom_++;
    if (focused_rom_ >= roms_.size()) focused_rom_ = 0;
    auto rom = roms_[focused_rom_];
    focus_rom(rom);
  }

  void previous() {
    // protect since this function is called from another thread context
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if (roms_.size() == 0) {
      return;
    }
    // focus the previous rom
    focused_rom_--;
    if (focused_rom_ < 0) focused_rom_ = roms_.size() - 1;
    auto rom = roms_[focused_rom_];
    focus_rom(rom);
  }

  void focus_rom(lv_obj_t *new_focus, bool scroll_to_view=true) {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if (roms_.size() == 0) {
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

    if (scroll_to_view) {
      lv_obj_scroll_to_view(new_focus, LV_ANIM_ON);
    }

    // update the boxart
    auto boxart_path = boxart_paths_[focused_rom_].c_str();
    focused_boxart_ = make_boxart(boxart_path);
    lv_img_set_src(ui_boxart, &focused_boxart_);
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

  void update_haptic_waveform_label() {
    auto haptic_label = fmt::format("{}", haptic_waveform_);
    lv_label_set_text(ui_hapticlabel, haptic_label.c_str());
  }

protected:
  void init_ui() {
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

    lv_bar_set_value(ui_volumebar, audio_level_, LV_ANIM_OFF);

    // rom screen navigation
    lv_obj_add_event_cb(ui_settingsbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
    lv_obj_add_event_cb(ui_playbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

    // volume settings
    lv_obj_add_event_cb(ui_volumeupbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
    lv_obj_add_event_cb(ui_volumedownbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
    lv_obj_add_event_cb(ui_mutebutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

    // haptic settings
    lv_obj_add_event_cb(ui_hapticdownbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
    lv_obj_add_event_cb(ui_hapticupbutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
    lv_obj_add_event_cb(ui_hapticplaybutton, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
    // ensure the waveform is set and the ui is updated
    set_haptic_waveform(haptic_waveform_);
  }

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

  void update(std::mutex& m, std::condition_variable& cv) {
    if (!paused_) {
      std::lock_guard<std::recursive_mutex> lk(mutex_);
      lv_task_handler();
    }
    {
      using namespace std::chrono_literals;
      std::unique_lock<std::mutex> lk(m);
      cv.wait_for(lk, 16ms);
    }
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
      gui->on_pressed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e) {
    lv_obj_t * target = lv_event_get_target(e);
    logger_.info("PRESSED: {}", fmt::ptr(target));
    // is it the settings button?
    bool is_settings_button = (target == ui_settingsbutton);
    if (is_settings_button) {
      // TODO: DO SOMETHING HERE!
      return;
    }
    // volume controls
    bool is_volume_up_button = (target == ui_volumeupbutton);
    if (is_volume_up_button) {
      set_audio_level(audio_level_ + 10);
      return;
    }
    bool is_volume_down_button = (target == ui_volumedownbutton);
    if (is_volume_down_button) {
      set_audio_level(audio_level_ - 10);
      return;
    }
    bool is_mute_button = (target == ui_mutebutton);
    if (is_mute_button) {
      toggle_mute();
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
      return;
    }
    // or is it one of the roms?
    if (std::find(roms_.begin(), roms_.end(), target) != roms_.end()) {
      // it's one of the roms, focus it! this was pressed, so don't scroll (it
      // will already scroll)
      focus_rom(target, false);
    }
  }

  // LVLG gui objects
  std::atomic<bool> muted_{false};
  std::atomic<int> audio_level_{60};
  std::vector<std::string> boxart_paths_;
  std::vector<lv_obj_t*> roms_;
  std::atomic<int> focused_rom_{-1};
  lv_img_dsc_t focused_boxart_;

  lv_anim_t rom_label_animation_template_;
  lv_style_t rom_label_style_;

  Jpeg decoder_;

  play_haptic_fn play_haptic_;
  set_waveform_fn set_waveform_;
  std::atomic<int> haptic_waveform_{16}; // for the DRV2605, this is a 1s alert

  std::atomic<bool> ready_to_play_{false};
  std::atomic<bool> paused_{false};
  std::shared_ptr<espp::Display> display_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::recursive_mutex mutex_;
};
