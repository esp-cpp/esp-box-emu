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

class Cart {
public:

  struct Config {
    RomInfo info;
    std::shared_ptr<espp::Display> display;
    espp::Logger::Verbosity verbosity = espp::Logger::Verbosity::WARN;
  };

  Cart(const Config& config)
    : info_(config.info),
      menu_({.display = config.display}),
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
      while (true) {
        using namespace std::chrono_literals;
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
    menu_.resume();
  }

  virtual void hide_menu() {
    menu_.pause();
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
  Menu menu_;
  std::recursive_mutex menu_mutex_;
  std::shared_ptr<espp::Display> display_;
  espp::Logger logger_;
};
