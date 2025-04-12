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

  virtual ~MsxCart() override {
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
    init_msx(get_rom_filename(), romdata_, rom_size_bytes_);
#endif
  }

  void deinit() {
#if defined(ENABLE_MSX)
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
  static constexpr size_t MSX_HEIGHT = 228;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_MSX)
    logger_.info("msx::pre_menu()");
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_MSX)
    logger_.info("msx::post_menu()");
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_MSX)
    logger_.info("msx::video: original");
    BoxEmu::get().display_size(MSX_WIDTH, MSX_HEIGHT);
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
    float x_scale = static_cast<float>(SCREEN_HEIGHT) / static_cast<float>(MSX_HEIGHT);
    int new_width = static_cast<int>(static_cast<float>(MSX_WIDTH) * x_scale);
    BoxEmu::get().display_size(new_width, SCREEN_HEIGHT);
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_MSX)
    logger_.info("msx::video: fill");
    BoxEmu::get().display_size(SCREEN_WIDTH, SCREEN_HEIGHT);
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
