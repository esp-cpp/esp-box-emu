#pragma once

#include "cart.hpp"
#if defined(ENABLE_PICO8)
#include "pico8.hpp"
#endif

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
    load_pico8_state(get_save_path().c_str());
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void save() override {
    Cart::save();
#if defined(ENABLE_PICO8)
    save_pico8_state(get_save_path(true).c_str());
#endif
  }

  void init() {
#if defined(ENABLE_PICO8)
    init_pico8(get_rom_filename().c_str(), romdata_, rom_size_bytes_);
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
    // Update input
    uint8_t buttons = 0;
    auto input = get_input();
    if (input.left) buttons |= PICO8_BTN_LEFT;
    if (input.right) buttons |= PICO8_BTN_RIGHT;
    if (input.up) buttons |= PICO8_BTN_UP;
    if (input.down) buttons |= PICO8_BTN_DOWN;
    if (input.a) buttons |= PICO8_BTN_Z;      // Primary action
    if (input.b) buttons |= PICO8_BTN_X;      // Secondary action
    if (input.start) buttons |= PICO8_BTN_ENTER;
    if (input.select) buttons |= PICO8_BTN_SHIFT;
    
    set_pico8_input(buttons);
    
    // Run one frame
    run_pico8_frame();
#endif
    return Cart::run();
  }

protected:
  // PICO-8 native resolution
  static constexpr size_t PICO8_WIDTH = 128;
  static constexpr size_t PICO8_HEIGHT = 128;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_PICO8)
    logger_.info("pico8::pre_menu()");
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_PICO8)
    logger_.info("pico8::post_menu()");
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_PICO8)
    logger_.info("pico8::video: original");
    BoxEmu::get().display_size(PICO8_WIDTH, PICO8_HEIGHT);
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(PICO8_WIDTH, PICO8_HEIGHT);
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
    logger_.info("pico8::video: fit");
    // PICO-8 is square, so fit to smallest screen dimension
    int min_dimension = std::min(SCREEN_WIDTH, SCREEN_HEIGHT);
    BoxEmu::get().display_size(min_dimension, min_dimension);
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_PICO8)
    logger_.info("pico8::video: fill");
    BoxEmu::get().display_size(SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
  }

  virtual std::string get_save_extension() const override {
    return "_p8.sav";
  }
};