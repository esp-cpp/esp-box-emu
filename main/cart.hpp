#pragma once

#include <filesystem>

#include "fs_init.hpp"
#include "input.h"
#include "logger.hpp"
#include "mmap.hpp"
#include "rom_info.hpp"
#include "st7789.hpp"

#include "gameboy.hpp"
#include "nes.hpp"

// GB
static constexpr size_t GAMEBOY_WIDTH = 160;
static constexpr size_t GAMEBOY_HEIGHT = 144;
// SMS
static constexpr size_t SMS_WIDTH = 256;
static constexpr size_t SMS_HEIGHT = 192;
// GG
static constexpr size_t GAMEGEAR_WIDTH = 160;
static constexpr size_t GAMEGEAR_HEIGHT = 144;

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

  bool load() {
    logger_.info("load");
    return true;
  }

  bool save() {
    logger_.info("save");
    return true;
  }

  void init() {
    logger_.info("init");
    can_run_ = false;
    espp::St7789::clear(0,0,320,240);
    // TODO: show loading text / graphic?
    // copy the rom data
    auto rom_filename = get_rom_filename();
    size_t rom_size_bytes = copy_romdata_to_cart_partition(rom_filename);
    if (!rom_size_bytes) {
      logger_.error("Could not copy {} into cart partition!", rom_filename);
      return;
    }
    uint8_t* romdata = get_mmapped_romdata();
    // now actually init the emulation using the copied romdata
    switch(info_.platform) {
    case Emulator::GAMEBOY:
    case Emulator::GAMEBOY_COLOR:
      logger_.debug("Initializing GB/C");
      init_gameboy(rom_filename, romdata, rom_size_bytes);
      break;
    case Emulator::NES:
      logger_.debug("Initializing NES");
      init_nes(rom_filename, romdata, rom_size_bytes);
      break;
    default:
      logger_.warn("Unknown cart type!");
      break;
    }
    logger_.info("Init complete");
    can_run_ = true;
  }

  void deinit() {
    logger_.info("deinit");
    // TODO: save or prompt to save here?
    // TODO: show quitting text / graphic?
    switch(info_.platform) {
    case Emulator::GAMEBOY:
    case Emulator::GAMEBOY_COLOR:
      deinit_gameboy();
      break;
    case Emulator::NES:
      deinit_nes();
      break;
    default:
      break;
    }
    can_run_ = false;
  }

  void run() {
    logger_.info("run");
    switch(info_.platform) {
    case Emulator::GAMEBOY:
    case Emulator::GAMEBOY_COLOR:
      start_gameboy_tasks();
      while (can_run_ && !user_quit()) {
        run_gameboy_rom();
      }
      stop_gameboy_tasks();
      break;
    case Emulator::NES:
      while (can_run_ && !user_quit()) {
        run_nes_rom();
      }
      break;
    default:
      break;
    }
  }

protected:
  static const std::string FS_PREFIX;
  static const std::string savedir;

  std::string get_save_path(bool bypass_exist_check=false) {
    namespace fs = std::filesystem;
    logger_.info("Save directory: {}", savedir);
    fs::create_directories(savedir);
    auto save_path = savedir + "/" + fs::path(get_rom_filename()).stem().string();
    switch (info_.platform) {
    case Emulator::GAMEBOY:
      save_path += "_gb.sav";
      break;
    case Emulator::GAMEBOY_COLOR:
      save_path += "_gbc.sav";
      break;
    case Emulator::NES:
      save_path += "_nes.sav";
      break;
    default:
      break;
    }
    if (bypass_exist_check || fs::exists(save_path)) {
      logger_.info("found: {}", save_path);
      return save_path;
    } else {
      logger_.warn("Could not find {}", save_path);
    }
    return "";
  }

  size_t selected_slot_{0};
  bool can_run_{false};
  RomInfo info_;
  espp::Logger logger_;
};
