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
    stop_gameboy_tasks();
    load_gameboy(get_save_path());
    start_gameboy_tasks();
  }

  virtual void save() override {
    stop_gameboy_tasks();
    save_gameboy(get_save_path(true));
    start_gameboy_tasks();
  }

  virtual void init() override {
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
