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
#include "menu.hpp"

/// This class is the base class for all carts.
/// It provides the following functionality:
/// - menu
/// - save/load
/// - screenshot
/// - video setting
/// - romdata
class Cart {
public:

  /// Configuration for the Cart class
  struct Config {
    RomInfo info; ///< rom info
    std::shared_ptr<espp::Display> display; ///< display pointer for the menu
    espp::Logger::Verbosity verbosity = espp::Logger::Verbosity::WARN; ///< verbosity level for the logger
  };

  /// Constructor
  /// \param config configuration for the cart
  Cart(const Config& config)
    : info_(config.info),
      menu_({
        .display = config.display,
        .action_callback =
        std::bind(&Cart::on_menu_action, this, std::placeholders::_1)
      }),
      display_(config.display),
      logger_({.tag = "Cart", .level = config.verbosity}) {
  }

  ~Cart() {
    deinit();
  }

  std::string get_rom_filename() const {
    return FS_PREFIX + "/" + info_.rom_path;
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
    handle_video_setting();
  }

  virtual void deinit() {
    logger_.info("deinit");
  }

  virtual bool run() {
    running_ = true;
    // handle touchpad so we can know if the user presses the menu
    uint8_t _num_touches, _btn_state;
    uint16_t _x,_y;
    touchpad_read(&_num_touches, &_x, &_y, &_btn_state);
    if (_btn_state) {
      logger_.warn("Menu pressed!");
      pre_menu();
      menu_.resume();
      display_->force_refresh();
      display_->resume();
      // wait here until the menu is no longer shown
      while (!menu_.is_paused()) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(100ms);
      }
      display_->pause();
      // make sure to clear the screen before we resume the game
      espp::St7789::clear(0,0,320,240);
      post_menu();
    }
    return running_;
  }

protected:
  static const std::string FS_PREFIX;
  static const std::string savedir;

  virtual void on_menu_action(Menu::Action action) {
    switch (action) {
    case Menu::Action::RESUME:
      menu_.pause();
      break;
    case Menu::Action::RESET:
      menu_.pause();
      break;
    case Menu::Action::QUIT:
      running_ = false;
      menu_.pause();
      break;
    case Menu::Action::SAVE:
      save();
      break;
    case Menu::Action::LOAD:
      load();
      break;
    default:
      break;
    }
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
    handle_video_setting();
  }

  // subclass should override these methods
  virtual void set_original_video_setting() = 0;
  virtual void set_fit_video_setting() = 0;
  virtual void set_fill_video_setting() = 0;

  virtual void handle_video_setting() {
    logger_.info("handling video setting...");
    switch (get_video_setting()) {
    case VideoSetting::ORIGINAL:
      set_original_video_setting();
      break;
    case VideoSetting::FIT:
      set_fit_video_setting();
      break;
    case VideoSetting::FILL:
      set_fill_video_setting();
      break;
    default:
      break;
    }
  }

  std::string get_save_path(bool bypass_exist_check=false) {
    namespace fs = std::filesystem;
    logger_.info("Save directory: {}", savedir);
    fs::create_directories(savedir);
    auto save_path =
      savedir + "/" +
      fs::path(get_rom_filename()).stem().string() +
      fmt::format("_{}", menu_.get_selected_slot()) +
      get_save_extension();
    if (bypass_exist_check || fs::exists(save_path)) {
      logger_.info("found: {}", save_path);
      return save_path;
    } else {
      logger_.warn("Could not find {}", save_path);
    }
    return "";
  }

  std::string get_screenshot_path(bool bypass_exist_check=false) {
    namespace fs = std::filesystem;
    logger_.info("Save directory: {}", savedir);
    fs::create_directories(savedir);
    auto save_path =
      savedir + "/" +
      fs::path(get_rom_filename()).stem().string() +
      fmt::format("_{}", menu_.get_selected_slot()) +
      ".jpg";
    if (bypass_exist_check || fs::exists(save_path)) {
      logger_.info("found: {}", save_path);
      return save_path;
    } else {
      logger_.warn("Could not find {}", save_path);
    }
    return "";
  }

  std::atomic<bool> running_{false};
  size_t rom_size_bytes_{0};
  uint8_t* romdata_{nullptr};
  RomInfo info_;
  Menu menu_;
  std::recursive_mutex menu_mutex_;
  std::shared_ptr<espp::Display> display_;
  espp::Logger logger_;
};
