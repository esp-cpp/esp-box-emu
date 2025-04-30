#pragma once

#include "cart.hpp"
#if defined(ENABLE_GBC)
#include "gameboy.hpp"
#endif

class GbcCart : public Cart {
public:

  explicit GbcCart(const Cart::Config& config)
    : Cart(config) {
    handle_video_setting();
    init();
  }

  virtual ~GbcCart() override {
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

  void init() {
#if defined(ENABLE_GBC)
    init_gameboy(get_rom_filename(), romdata_, rom_size_bytes_);
#endif
  }

  void deinit() {
#if defined(ENABLE_GBC)
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
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_GBC)
    logger_.info("gbc::post_menu()");
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_GBC)
    logger_.info("gbc::video: original");
    BoxEmu::get().display_size(GAMEBOY_WIDTH, GAMEBOY_HEIGHT);
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(GAMEBOY_WIDTH, GAMEBOY_HEIGHT);
  }

  // cppcheck-suppress uselessOverride
  virtual std::span<uint8_t> get_video_buffer() const override {
#if defined(ENABLE_GBC)
    return get_gameboy_video_buffer();
#else
    return std::span<uint8_t>();
#endif
  }

  virtual void set_fit_video_setting() override {
#if defined(ENABLE_GBC)
    logger_.info("gbc::video: fit");
    float x_scale = static_cast<float>(SCREEN_HEIGHT) / static_cast<float>(GAMEBOY_HEIGHT);
    int new_width = static_cast<int>(static_cast<float>(GAMEBOY_WIDTH) * x_scale);
    BoxEmu::get().display_size(new_width, SCREEN_HEIGHT);
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_GBC)
    logger_.info("gbc::video: fill");
    BoxEmu::get().display_size(SCREEN_WIDTH, SCREEN_HEIGHT);
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
