#pragma once

#include <filesystem>

#include "fs_init.hpp"
#include "input.h"
#include "logger.hpp"
#include "mmap.hpp"
#include "rom_info.hpp"
#include "st7789.hpp"

class Cart {
public:

  Cart(const RomInfo& info, espp::Logger::Verbosity verbosity = espp::Logger::Verbosity::WARN)
    : info_(info),
      logger_({.tag = "Cart", .level = verbosity}) {
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
      // TODO: for now we're simply handling the button press as a quit action,
      // so we return false from the run function to indicate that we should stop.
      return false;
      // TODO: show a menu here that will allow the user to:
      // * save state
      // * load state
      // * select slot (with image?)
      // * change volume
      // * change video scaling
      // * exit menu
      // * quit emulation
    }
    return true;
  }

protected:
  static const std::string FS_PREFIX;
  static const std::string savedir;

  virtual std::string get_save_extension() const {
    return ".sav";
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
  espp::Logger logger_;
};
