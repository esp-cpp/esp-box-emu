#pragma once

#include <filesystem>
#include <memory>
#include <mutex>

#include "display.hpp"
#include "fs_init.hpp"
#include "input.h"
#include "logger.hpp"
#include "mmap.hpp"
#include "rom_info.hpp"
#include "st7789.hpp"

class Cart {
public:

  struct Config {
    RomInfo info;
    std::shared_ptr<espp::Display> display;
    espp::Logger::Verbosity verbosity = espp::Logger::Verbosity::WARN;
  };

  Cart(const Config& config)
    : info_(config.info),
      display_(config.display),
      logger_({.tag = "Cart", .level = config.verbosity}) {
    init();
  }

  ~Cart() {
    deinit();
  }

  std::string get_rom_filename() const {
    return FS_PREFIX + "/" + info_.rom_path;
  }

  size_t get_selected_slot() const {
    return selected_slot_;
  }

  void select_slot(size_t slot) {
    selected_slot_ = slot;
  }

  virtual void load() {
    logger_.info("load");
  }

  virtual void save() {
    logger_.info("save");
  }

  virtual void init() {
    logger_.info("init");
    espp::St7789::clear(0,0,320,240);
    // copy the romdata
    rom_size_bytes_ = copy_romdata_to_cart_partition(get_rom_filename());
    romdata_ = get_mmapped_romdata();
  }

  virtual void deinit() {
    logger_.info("deinit");
  }

  virtual bool run() {
    // handle touchpad so we can know if the user presses the menu
    uint8_t _num_touches, _btn_state;
    uint16_t _x,_y;
    touchpad_read(&_num_touches, &_x, &_y, &_btn_state);
    if (_btn_state) {
      logger_.warn("Menu pressed!");
      pre_menu();
      display_->resume();
      // TODO: show a menu here that will allow the user to:
      show_menu();
      // * save state
      // * load state
      // * select slot (with image?)
      // * change volume
      // * change video scaling
      // * exit menu
      // * quit emulation
      // wait here until the menu is no longer shown
      while (menu_active_) {
        using namespace std::chrono_literals;
        std::unique_lock<std::recursive_mutex> lk(menu_mutex_);
        lv_task_handler();
        std::this_thread::sleep_for(16ms);
      }
      hide_menu();
      display_->pause();
      post_menu();
    }
    return true;
  }

protected:
  static const std::string FS_PREFIX;
  static const std::string savedir;

  virtual void show_menu() {
    // Create a background object that covers the entire screen
    lv_obj_t *bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_add_flag(bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    // Create a container for the modal menu
    lv_obj_t *menu_cont = lv_obj_create(bg);
    lv_obj_set_size(menu_cont, LV_HOR_RES/2, (LV_VER_RES * 4) / 5);
    lv_obj_center(menu_cont);

    // Create a label for the menu title
    lv_obj_t *label = lv_label_create(menu_cont);
    lv_label_set_text(label, "Emulation Paused");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);

    // Create a button for the menu
    lv_obj_t *btn = lv_btn_create(menu_cont);
    lv_obj_add_event_cb(btn, &Cart::event_cb, LV_EVENT_PRESSED, static_cast<void*>(this));
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_width(btn, 120);

    // Create a label for the button
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Resume");

    menu_active_ = true;
  }

  virtual void hide_menu() {
    menu_active_ = false;
  }

  static void event_cb(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    auto user_data = lv_event_get_user_data(e);
    auto cart = static_cast<Cart*>(user_data);
    if (!cart) {
      return;
    }
    switch (event_code) {
    case LV_EVENT_SHORT_CLICKED:
      break;
    case LV_EVENT_PRESSED:
      cart->on_pressed(e);
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
    menu_active_ = false;
  }

  bool update(std::mutex& m, std::condition_variable& cv) {
    if (menu_active_) {
      std::lock_guard<std::recursive_mutex> lk(menu_mutex_);
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

  virtual std::string get_save_extension() const {
    return ".sav";
  }

  virtual void pre_menu() {
    // subclass should override this function if they need to stop their tasks
    // or save screen state before the menu is shown
  }

  virtual void post_menu() {
    // subclass should override this function if they need to resume their tasks
    // or restore screen state before the menu is shown
  }

  std::string get_save_path(bool bypass_exist_check=false) {
    namespace fs = std::filesystem;
    logger_.info("Save directory: {}", savedir);
    fs::create_directories(savedir);
    auto save_path =
      savedir + "/" +
      fs::path(get_rom_filename()).stem().string() +
      get_save_extension();
    if (bypass_exist_check || fs::exists(save_path)) {
      logger_.info("found: {}", save_path);
      return save_path;
    } else {
      logger_.warn("Could not find {}", save_path);
    }
    return "";
  }

  size_t selected_slot_{0};
  size_t rom_size_bytes_{0};
  uint8_t* romdata_{nullptr};
  RomInfo info_;
  std::atomic<bool> menu_active_{false};
  std::recursive_mutex menu_mutex_;
  std::shared_ptr<espp::Display> display_;
  espp::Logger logger_;
};
