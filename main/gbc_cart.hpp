#pragma once

#include "cart.hpp"
#include "gameboy.hpp"

class GbcCart : public Cart {
public:

  GbcCart(const Cart::Config& config)
    : Cart(config) {
    init();
  }

  ~GbcCart() {
    deinit();
  }

  virtual void load() override {
    Cart::load();
    load_gameboy(get_save_path());
  }

  virtual void save() override {
    Cart::save();
    save_gameboy(get_save_path(true));
  }

  virtual void init() override {
    Cart::init();
    init_gameboy(get_rom_filename(), romdata_, rom_size_bytes_);
    start_gameboy_tasks();
  }

  virtual void deinit() override {
    stop_gameboy_tasks();
    deinit_gameboy();
  }

  virtual bool run() override {
    run_gameboy_rom();
    return Cart::run();
  }

protected:
  // GB
  static constexpr size_t GAMEBOY_WIDTH = 160;
  static constexpr size_t GAMEBOY_HEIGHT = 144;

  virtual void pre_menu() override {
    Cart::pre_menu();
    logger_.info("gbc::pre_menu()");
    stop_gameboy_tasks();
  }

  virtual void post_menu() override {
    Cart::post_menu();
    logger_.info("gbc::post_menu()");
    start_gameboy_tasks();
  }

  virtual void set_original_video_setting() override {
    logger_.info("gbc::video: original");
    set_gb_video_original();
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(GAMEBOY_WIDTH, GAMEBOY_HEIGHT);
  }

  virtual void set_fit_video_setting() override {
    logger_.info("gbc::video: fit");
    set_gb_video_fit();
  }

  virtual void set_fill_video_setting() override {
    logger_.info("gbc::video: fill");
    set_gb_video_fill();
  }

  virtual std::string get_save_extension() const override {
    switch (info_.platform) {
    case Emulator::GAMEBOY:
      return "_gb.sav";
    case Emulator::GAMEBOY_COLOR:
      return "_gbc.sav";
    default:
      return Cart::get_save_extension();
    }
  }
};
