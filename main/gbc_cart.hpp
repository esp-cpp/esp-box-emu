#pragma once

#include "cart.hpp"
#if defined(ENABLE_GBC)
#include "gameboy.hpp"
#endif

class GbcCart : public Cart {
public:

  explicit GbcCart(const Cart::Config& config)
    : Cart(config) {
    init();
  }

  ~GbcCart() {
    deinit();
  }

  // cppcheck-suppress uselessOverride
  virtual void reset() override {
    Cart::reset();
#if defined(ENABLE_GBC)
    reset_gameboy();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void load() override {
    Cart::load();
#if defined(ENABLE_GBC)
    load_gameboy(get_save_path());
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void save() override {
    Cart::save();
#if defined(ENABLE_GBC)
    save_gameboy(get_save_path(true));
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void init() override {
    Cart::init();
#if defined(ENABLE_GBC)
    init_gameboy(get_rom_filename(), romdata_, rom_size_bytes_);
    start_gameboy_tasks();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void deinit() override {
    Cart::deinit();
#if defined(ENABLE_GBC)
    stop_gameboy_tasks();
    deinit_gameboy();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual bool run() override {
#if defined(ENABLE_GBC)
    run_gameboy_rom();
#endif
    return Cart::run();
  }

protected:
  // GB
  static constexpr size_t GAMEBOY_WIDTH = 160;
  static constexpr size_t GAMEBOY_HEIGHT = 144;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_GBC)
    logger_.info("gbc::pre_menu()");
    stop_gameboy_tasks();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_GBC)
    logger_.info("gbc::post_menu()");
    start_gameboy_tasks();
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_GBC)
    logger_.info("gbc::video: original");
    set_gb_video_original();
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(GAMEBOY_WIDTH, GAMEBOY_HEIGHT);
  }

  // cppcheck-suppress uselessOverride
  virtual std::vector<uint8_t> get_video_buffer() const override {
#if defined(ENABLE_GBC)
    return get_gameboy_video_buffer();
#else
    return std::vector<uint8_t>();
#endif
  }

  virtual void set_fit_video_setting() override {
#if defined(ENABLE_GBC)
    logger_.info("gbc::video: fit");
    set_gb_video_fit();
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_GBC)
    logger_.info("gbc::video: fill");
    set_gb_video_fill();
#endif
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
