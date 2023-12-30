#pragma once

#include "cart.hpp"
#if defined(ENABLE_MSX)
#include "msx.hpp"
#endif

class MsxCart : public Cart {
public:

  explicit MsxCart(const Cart::Config& config)
    : Cart(config) {
    handle_video_setting();
    init();
  }

  ~MsxCart() {
    logger_.info("~MsxCart()");
    deinit();
  }

  // cppcheck-suppress uselessOverride
  virtual void reset() override {
    Cart::reset();
#if defined(ENABLE_MSX)
    reset_msx();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void load() override {
    Cart::load();
#if defined(ENABLE_MSX)
    load_msx(get_save_path());
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void save() override {
    Cart::save();
#if defined(ENABLE_MSX)
    save_msx(get_save_path(true));
#endif
  }

  void init() {
#if defined(ENABLE_MSX)
    switch (info_.platform) {
    case Emulator::MSX:
      logger_.info("msx::init()");
      init_msx(romdata_, rom_size_bytes_);
      break;
    default:
      logger_.error("unknown platform");
      return;
    }
    start_msx_tasks();
#endif
  }

  void deinit() {
#if defined(ENABLE_MSX)
    stop_msx_tasks();
    deinit_msx();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual bool run() override {
#if defined(ENABLE_MSX)
    run_msx_rom();
#endif
    return Cart::run();
  }

protected:
  // MSX
  static constexpr size_t MSX_WIDTH = 256;
  static constexpr size_t MSX_HEIGHT = 192;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_MSX)
    logger_.info("msx::pre_menu()");
    stop_msx_tasks();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_MSX)
    logger_.info("msx::post_menu()");
    start_msx_tasks();
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_MSX)
    logger_.info("msx::video: original");
    set_msx_video_original();
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(MSX_WIDTH, MSX_HEIGHT);
  }

  // cppcheck-suppress uselessOverride
  virtual std::vector<uint8_t> get_video_buffer() const override {
#if defined(ENABLE_MSX)
    return get_msx_video_buffer();
#else
    return std::vector<uint8_t>();
#endif
  }

  virtual void set_fit_video_setting() override {
#if defined(ENABLE_MSX)
    logger_.info("msx::video: fit");
    set_msx_video_fit();
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_MSX)
    logger_.info("msx::video: fill");
    set_msx_video_fill();
#endif
  }

  virtual std::string get_save_extension() const override {
    switch (info_.platform) {
    case Emulator::MSX:
      return "_msx.sav";
    default:
      return Cart::get_save_extension();
    }
  }
};
