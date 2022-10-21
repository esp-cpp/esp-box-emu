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

  void add_rom(const std::string& name, const std::string& image_path) {
    std::scoped_lock<std::mutex> lk(mutex_);
    auto new_rom = ui_rom_create(rom_container_);
    lv_obj_center(new_rom);
    // set the rom's label text
    auto label = ui_comp_get_child(new_rom, UI_COMP_ROM_LABEL);
    lv_label_set_text(label, name.c_str());
    // set the rom's image
    auto image = ui_comp_get_child(new_rom, UI_COMP_ROM_IMAGE);
    lv_img_set_src(image, image_path.c_str());
    // and add it to our vector
    roms_.push_back(new_rom);
  }

  void pause() { paused_ = true; }
  void resume() { paused_ = false; }

  void next() {
    std::scoped_lock<std::mutex> lk(mutex_);
    if (roms_.size() == 0) {
      return;
    }
    // unfocus all roms
    for (auto rom : roms_) {
      lv_obj_clear_state(rom, LV_STATE_FOCUSED);
    }
    // focus the next rom
    focused_rom_++;
    if (focused_rom_ >= roms_.size()) focused_rom_ = 0;
    auto rom = roms_[focused_rom_];
    lv_obj_add_state(rom, LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(rom, LV_ANIM_ON);
  }

  void previous() {
    std::scoped_lock<std::mutex> lk(mutex_);
    if (roms_.size() == 0) {
      return;
    }
    // unfocus all roms
    for (auto rom : roms_) {
      lv_obj_clear_state(rom, LV_STATE_FOCUSED);
    }
    // focus the previous rom
    focused_rom_--;
    if (focused_rom_ < 0) focused_rom_ = roms_.size() - 1;
    auto rom = roms_[focused_rom_];
    lv_obj_add_state(rom, LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(rom, LV_ANIM_ON);
  }

protected:
  void init_ui() {
    ui_init();
    rom_container_ = lv_obj_create(lv_scr_act());
    lv_obj_set_size(rom_container_, display_->width(), display_->height() * 0.8f);
    lv_obj_set_scroll_snap_x(rom_container_, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_flex_flow(rom_container_, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(rom_container_, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_dir(rom_container_, LV_DIR_HOR);
    lv_obj_update_snap(rom_container_, LV_ANIM_ON);
    // lv_obj_set_style_pad_row(rom_container_, 5, 0);
    lv_obj_center(rom_container_);
  }

  void update(std::mutex& m, std::condition_variable& cv) {
    if (!paused_){
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
  lv_obj_t *rom_container_;
  std::vector<lv_obj_t*> roms_;
  int focused_rom_{0};

  std::atomic<bool> paused_{false};
  std::shared_ptr<espp::Display> display_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::mutex mutex_;
};
