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
    load_nes(get_save_path());
  }

  virtual void save() override {
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
