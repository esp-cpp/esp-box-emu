#pragma once

#include "cart.hpp"
#if defined(ENABLE_PICO8)
#include "pico8.hpp"
#endif

// PICO-8 carts (.p8 / .p8.png) through femto8.
class Pico8Cart : public Cart {
public:
  explicit Pico8Cart(const Cart::Config& config)
    : Cart(config) {
    handle_video_setting();
    init();
  }

  virtual ~Pico8Cart() override {
    deinit();
  }

  // cppcheck-suppress uselessOverride
  virtual void reset() override {
    Cart::reset();
#if defined(ENABLE_PICO8)
    reset_pico8();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void load() override {
    Cart::load();
#if defined(ENABLE_PICO8)
    load_pico8(get_save_path(), get_selected_save_slot());
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void save() override {
    Cart::save();
#if defined(ENABLE_PICO8)
    save_pico8(get_save_path(true), get_selected_save_slot());
#endif
  }

  void init() {
#if defined(ENABLE_PICO8)
    init_pico8(get_rom_filename(), romdata_, rom_size_bytes_);
#endif
  }

  void deinit() {
#if defined(ENABLE_PICO8)
    deinit_pico8();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual bool run() override {
#if defined(ENABLE_PICO8)
    run_pico8_rom();
    if (pico8_quit_requested()) {
      running_ = false;
      return false;
    }
#endif
    return Cart::run();
  }

protected:
  static constexpr size_t P8_WIDTH = 128;
  static constexpr size_t P8_HEIGHT = 128;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_PICO8)
    logger_.info("pico8::pre_menu()");
    pause_pico8_tasks();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_PICO8)
    logger_.info("pico8::post_menu()");
    resume_pico8_tasks();
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_PICO8)
    // 1:1 pixels (128x128 centered); "fit" is the sensible default
    logger_.info("pico8::video: original");
    BoxEmu::get().display_size(P8_WIDTH, P8_HEIGHT);
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(P8_WIDTH, P8_HEIGHT);
  }

  // cppcheck-suppress uselessOverride
  virtual std::span<uint8_t> get_video_buffer() const override {
#if defined(ENABLE_PICO8)
    return get_pico8_video_buffer();
#else
    return std::span<uint8_t>();
#endif
  }

  virtual void set_fit_video_setting() override {
#if defined(ENABLE_PICO8)
    // a square screen on the 4:3 panel: 240x240
    logger_.info("pico8::video: fit");
    BoxEmu::get().display_size(SCREEN_HEIGHT, SCREEN_HEIGHT);
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_PICO8)
    logger_.info("pico8::video: fill");
    BoxEmu::get().display_size(SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
  }

  virtual std::string get_save_extension() const override {
    return "_pico8.sav";
  }
};
