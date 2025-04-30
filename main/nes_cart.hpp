#pragma once

#include "cart.hpp"
#if defined(ENABLE_NES)
#include "nes.hpp"
#endif

class NesCart : public Cart {
public:

  explicit NesCart(const Cart::Config& config)
    : Cart(config) {
    handle_video_setting();
    init();
  }

  virtual ~NesCart() override {
    deinit();
  }

  // cppcheck-suppress uselessOverride
  virtual void reset() override {
    Cart::reset();
#if defined(ENABLE_NES)
    reset_nes();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void load() override {
    Cart::load();
#if defined(ENABLE_NES)
    load_nes(get_save_path());
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void save() override {
    Cart::save();
#if defined(ENABLE_NES)
    save_nes(get_save_path(true));
#endif
  }

  void init() {
#if defined(ENABLE_NES)
    init_nes(get_rom_filename(), romdata_, rom_size_bytes_);
#endif
  }

  void deinit() {
#if defined(ENABLE_NES)
    deinit_nes();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual bool run() override {
#if defined(ENABLE_NES)
    run_nes_rom();
#endif
    return Cart::run();
  }

protected:
  static constexpr size_t NES_WIDTH = 256;
  static constexpr size_t NES_HEIGHT = 240;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_NES)
    logger_.info("nes::pre_menu()");
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_NES)
    logger_.info("nes::post_menu()");
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(NES_WIDTH, NES_HEIGHT);
  }

  // cppcheck-suppress uselessOverride
  virtual std::span<uint8_t> get_video_buffer() const override {
#if defined(ENABLE_NES)
    return get_nes_video_buffer();
#else
    return std::span<uint8_t>();
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_NES)
    BoxEmu::get().display_size(NES_WIDTH, NES_HEIGHT);
#endif
  }

  virtual void set_fit_video_setting() override {
#if defined(ENABLE_NES)
    float x_scale = static_cast<float>(SCREEN_HEIGHT) / static_cast<float>(NES_HEIGHT);
    int new_width = static_cast<int>(static_cast<float>(NES_WIDTH) * x_scale);
    BoxEmu::get().display_size(new_width, SCREEN_HEIGHT);
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_NES)
    BoxEmu::get().display_size(SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
  }

  virtual std::string get_save_extension() const override {
    return "_nes.sav";
  }
};
