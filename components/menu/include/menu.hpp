#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "display.hpp"
#include "task.hpp"
#include "logger.hpp"

class Menu {
public:
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
  }

  ~Menu() {
    task_->stop();
    deinit_ui();
  }

  bool is_paused() { return paused_; }
  void pause() { paused_ = true; }
  void resume() { paused_ = false; }

protected:
  void init_ui();
  void deinit_ui();

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

  std::atomic<bool> paused_{true};
  std::shared_ptr<espp::Display> display_;
  action_fn action_callback_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::recursive_mutex mutex_;
};
