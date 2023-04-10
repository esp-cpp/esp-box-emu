#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "display.hpp"
#include "task.hpp"
#include "logger.hpp"

class Menu {
public:
  static constexpr size_t MAX_SLOT = 5;
  enum class Action { RESUME, RESET, SAVE, LOAD, QUIT };

  typedef std::function<void(Action)> action_fn;

  struct Config {
    std::shared_ptr<espp::Display> display;
    action_fn action_callback;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  enum class VideoSetting { ORIGINAL, FIT, FILL, MAX_UNUSED };

  Menu(const Config& config)
    : display_(config.display),
      action_callback_(config.action_callback),
      logger_({.tag="Menu", .level=config.log_level}) {
    init_ui();
    // now start the menu updater task
    using namespace std::placeholders;
    task_ = espp::Task::make_unique({
        .name = "Menu Task",
        .callback = std::bind(&Menu::update, this, _1, _2),
        .stack_size_bytes = 6 * 1024
      });
    task_->start();
    update_slot_display();
  }

  ~Menu() {
    task_->stop();
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

  bool is_paused() { return paused_; }
  void pause() { paused_ = true; }
  void resume() { paused_ = false; }

protected:
  void init_ui();
  void deinit_ui();
  void update_slot_display();
  void update_slot_label();
  void update_slot_image();

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
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);

  // LVLG menu objects
  std::atomic<bool> muted_{false};
  std::atomic<int> audio_level_{60};
  lv_img_dsc_t state_image_;
  lv_img_dsc_t paused_image_;

  int selected_slot_{0};
  std::atomic<bool> paused_{true};
  std::shared_ptr<espp::Display> display_;
  action_fn action_callback_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::recursive_mutex mutex_;
};
