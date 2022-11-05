#pragma once

#include <memory>
#include <mutex>
#include <vector>

extern "C" {
#include "ui.h"
#include "ui_comp.h"
}

#include "display.hpp"
#include "task.hpp"
#include "logger.hpp"

class Gui {
public:
  struct Config {
    std::shared_ptr<espp::Display> display;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  Gui(const Config& config) : display_(config.display), logger_({.tag="Gui", .level=config.log_level}) {
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

  void add_rom(const std::string& name, const std::string& image_path) {
    // make a new rom, which is a button with a label in it
    std::lock_guard<std::mutex> lk(mutex_);
    // make the rom's button
    auto new_rom = lv_btn_create(rom_container_);
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
    lv_obj_center(label);
    // and add it to our vector
    roms_.push_back(new_rom);
    boxarts_.push_back(image_path);
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
    lv_obj_t *rom;
    {
      // std::lock_guard<std::mutex> lk(mutex_);
      if (roms_.size() == 0) {
        return;
      }
      // focus the next rom
      focused_rom_++;
      if (focused_rom_ >= roms_.size()) focused_rom_ = 0;
      rom = roms_[focused_rom_];
    }
    focus_rom(rom);
  }

  void previous() {
    lv_obj_t *rom;
    {
      // std::lock_guard<std::mutex> lk(mutex_);
      if (roms_.size() == 0) {
        return;
      }
      // focus the previous rom
      focused_rom_--;
      if (focused_rom_ < 0) focused_rom_ = roms_.size() - 1;
      rom = roms_[focused_rom_];
    }
    focus_rom(rom);
  }

  void focus_rom(lv_obj_t *new_focus, bool scroll_to_view=true) {
    logger_.info("Focusing rom {}", fmt::ptr(new_focus));
    // std::lock_guard<std::mutex> lk(mutex_);
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
    lv_img_set_src(img_, boxarts_[focused_rom_].c_str());
  }

protected:
  void init_ui() {
    header_container_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header_container_, display_->width(), 75);
    lv_obj_align_to(header_container_, NULL, LV_ALIGN_TOP_MID, 0, 0);

    settings_button_ = lv_btn_create(header_container_);
    settings_button_label_ = lv_label_create(settings_button_);
    lv_label_set_text(settings_button_label_, LV_SYMBOL_SETTINGS);
    lv_obj_align(settings_button_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(settings_button_, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));

    header_label_ = lv_label_create(header_container_);
    lv_label_set_text(header_label_, "ESP EMU BOX");
    lv_obj_center(header_label_);

    play_button_ = lv_btn_create(header_container_);
    play_button_label_ = lv_label_create(play_button_);
    lv_label_set_text(play_button_label_, LV_SYMBOL_PLAY);
    lv_obj_align(play_button_, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(play_button_, &Gui::event_callback, LV_EVENT_PRESSED, static_cast<void*>(this));
    lv_obj_add_state(play_button_, LV_STATE_CHECKED);

    page_container_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page_container_, display_->width(), display_->height() - 75);
    lv_obj_align_to(page_container_, NULL, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_top(page_container_, 0, LV_STATE_DEFAULT );
    lv_obj_set_style_pad_bottom(page_container_, 0, LV_STATE_DEFAULT );
    lv_obj_set_style_pad_left(page_container_, 0, LV_STATE_DEFAULT );
    lv_obj_set_style_pad_right(page_container_, 0, LV_STATE_DEFAULT );

    rom_container_ = lv_obj_create(page_container_);
    lv_obj_set_size(rom_container_, display_->width() - 100, LV_PCT(100));
    lv_obj_set_scroll_snap_y(rom_container_, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_flex_flow(rom_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(rom_container_, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_dir(rom_container_, LV_DIR_VER);
    lv_obj_update_snap(rom_container_, LV_ANIM_ON);
    lv_obj_align(rom_container_, LV_ALIGN_LEFT_MID, 0, 0);

    img_ = lv_img_create(page_container_);
    lv_obj_set_width(img_, 100);
    lv_obj_align(img_, LV_ALIGN_RIGHT_MID, 0, 0);
  }

  void update(std::mutex& m, std::condition_variable& cv) {
    if (!paused_) {
      std::lock_guard<std::mutex> lk(mutex_);
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
    bool is_settings_button = (target == settings_button_);
    if (is_settings_button) {
      // TODO: DO SOMETHING HERE!
      return;
    }
    // or is it the play button?
    bool is_play_button = (target == play_button_);
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
  lv_obj_t *header_container_;
  lv_obj_t *header_label_;
  lv_obj_t *settings_button_;
  lv_obj_t *settings_button_label_;
  lv_obj_t *play_button_;
  lv_obj_t *play_button_label_;
  lv_obj_t *page_container_;
  lv_obj_t *rom_container_;
  lv_obj_t *img_;
  std::vector<std::string> boxarts_;
  std::vector<lv_obj_t*> roms_;
  int focused_rom_{-1};

  std::atomic<bool> ready_to_play_{false};
  std::atomic<bool> paused_{false};
  std::shared_ptr<espp::Display> display_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::mutex mutex_;
};
