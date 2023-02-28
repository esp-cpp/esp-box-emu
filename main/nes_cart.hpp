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
    init_nes(get_rom_filename(), romdata_, rom_size_bytes_);
  }

  virtual void deinit() override {
    deinit_nes();
  }


  virtual bool run() override {
    run_nes_rom();
    return Cart::run();
  }

protected:
  virtual std::string get_save_extension() const override {
    return "_nes.sav";
  }
};
