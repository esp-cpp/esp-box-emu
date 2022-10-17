#pragma once

#include <memory>
#include <mutex>

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

  void set_label(const std::string_view& label) {
    std::scoped_lock<std::mutex> lk(mutex_);
    lv_label_set_text(label_, label.data());
  }

  void set_meter(size_t value, bool animate=true) {
    std::scoped_lock<std::mutex> lk(mutex_);
    if (animate) {
      lv_bar_set_value(meter_, value, LV_ANIM_ON);
    } else {
      lv_bar_set_value(meter_, value, LV_ANIM_OFF);
    }
  }

protected:
  void init_ui() {
    // Create a container with COLUMN flex direction
    column_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(column_, display_->width(), display_->height());
    lv_obj_set_flex_flow(column_, LV_FLEX_FLOW_COLUMN);

    label_ = lv_label_create(column_);
    lv_label_set_text(label_, "Hello world");

    meter_ = lv_bar_create(lv_scr_act());
    lv_obj_set_size(meter_, display_->width() * 0.8f, 20);
    lv_obj_center(meter_);

    static lv_style_t style_indic;
    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);

    lv_obj_add_style(meter_, &style_indic, LV_PART_INDICATOR);
  }

  void update(std::mutex& m, std::condition_variable& cv) {
    {
      std::scoped_lock<std::mutex> lk(mutex_);
      lv_task_handler();
    }
    {
      using namespace std::chrono_literals;
      std::unique_lock<std::mutex> lk(m);
      cv.wait_for(lk, 16ms);
    }
  }

  // LVLG gui objects
  lv_obj_t *column_;
  lv_obj_t *label_;
  lv_obj_t *meter_;

  std::shared_ptr<espp::Display> display_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::mutex mutex_;
};
