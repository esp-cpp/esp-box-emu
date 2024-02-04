#pragma once

#include "cart.hpp"
#if defined(ENABLE_GENESIS)
#include "genesis.hpp"
#endif

class GenesisCart : public Cart {
public:

  explicit GenesisCart(const Cart::Config& config)
    : Cart(config) {
    handle_video_setting();
    init();
  }

  ~GenesisCart() {
    logger_.info("~GenesisCart()");
    deinit();
  }

  // cppcheck-suppress uselessOverride
  virtual void reset() override {
    Cart::reset();
#if defined(ENABLE_GENESIS)
    reset_genesis();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void load() override {
    Cart::load();
#if defined(ENABLE_GENESIS)
    load_genesis(get_save_path());
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void save() override {
    Cart::save();
#if defined(ENABLE_GENESIS)
    save_genesis(get_save_path(true));
#endif
  }

  void init() {
#if defined(ENABLE_GENESIS)
    switch (info_.platform) {
    case Emulator::SEGA_GENESIS:
    case Emulator::SEGA_MEGA_DRIVE:
      logger_.info("genesis::init()");
      init_genesis(romdata_, rom_size_bytes_);
      break;
    default:
      logger_.error("unknown platform");
      return;
    }
#endif
  }

  void deinit() {
#if defined(ENABLE_GENESIS)
    deinit_genesis();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual bool run() override {
#if defined(ENABLE_GENESIS)
    run_genesis_rom();
#endif
    return Cart::run();
  }

protected:
  // GENESIS
  static constexpr size_t GENESIS_WIDTH = 320;
  static constexpr size_t GENESIS_HEIGHT = 224;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_GENESIS)
    logger_.info("genesis::pre_menu()");
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_GENESIS)
    logger_.info("genesis::post_menu()");
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_GENESIS)
    auto height = GENESIS_HEIGHT;
    auto width = GENESIS_WIDTH;
    hal::set_display_size(width, height);
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    auto height = GENESIS_HEIGHT;
    auto width = GENESIS_WIDTH;
    return std::make_pair(width, height);
  }

  // cppcheck-suppress uselessOverride
  virtual std::vector<uint8_t> get_video_buffer() const override {
#if defined(ENABLE_GENESIS)
    return get_genesis_video_buffer();
#else
    return std::vector<uint8_t>();
#endif
  }

  virtual void set_fit_video_setting() override {
#if defined(ENABLE_GENESIS)
    logger_.info("genesis::video: fit");
    float height = GENESIS_HEIGHT;
    float width = GENESIS_WIDTH;
    float x_scale = static_cast<float>(SCREEN_HEIGHT) / height;
    int new_width = static_cast<int>(width * x_scale);
    hal::set_display_size(new_width, SCREEN_HEIGHT);
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_GENESIS)
    logger_.info("genesis::video: fill");
    hal::set_display_size(SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
  }

  virtual std::string get_save_extension() const override {
    switch (info_.platform) {
    case Emulator::SEGA_GENESIS:
    case Emulator::SEGA_MEGA_DRIVE:
      return "_genesis.sav";
    default:
      return Cart::get_save_extension();
    }
  }
};
