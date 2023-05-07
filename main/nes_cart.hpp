#pragma once

#include "cart.hpp"
#include "nes.hpp"

class NesCart : public Cart {
public:

  NesCart(const Cart::Config& config)
    : Cart(config) {
    init();
  }

  ~NesCart() {
    deinit();
  }

  virtual void load() override {
    Cart::load();
    load_nes(get_save_path());
  }

  virtual void save() override {
    Cart::save();
    save_nes(get_save_path(true));
  }

  virtual void init() override {
    Cart::init();
    init_nes(get_rom_filename(), romdata_, rom_size_bytes_);
    start_nes_tasks();
  }

  virtual void deinit() override {
    stop_nes_tasks();
    deinit_nes();
  }

  virtual bool run() override {
    run_nes_rom();
    return Cart::run();
  }

protected:
  static constexpr size_t NES_WIDTH = 256;
  static constexpr size_t NES_HEIGHT = 240;

  virtual void pre_menu() override {
    Cart::pre_menu();
    logger_.info("nes::pre_menu()");
    stop_nes_tasks();
  }

  virtual void post_menu() override {
    Cart::post_menu();
    logger_.info("nes::post_menu()");
    start_nes_tasks();
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(NES_WIDTH, NES_HEIGHT);
  }

  virtual void set_original_video_setting() override {
    set_nes_video_original();
  }

  virtual void set_fit_video_setting() override {
    set_nes_video_fit();
  }

  virtual void set_fill_video_setting() override {
    set_nes_video_fill();
  }

  virtual std::string get_save_extension() const override {
    return "_nes.sav";
  }
};
