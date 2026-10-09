#pragma once

#include "cart.hpp"
#if defined(ENABLE_MGS)
#include "mgs.hpp"
#endif

// Metal Gear Solid (native port of the decompilation). The "ROM" is STAGE.DIR;
// the rest of the disc data sits next to it (see README).
class MgsCart : public Cart {
public:
  explicit MgsCart(const Cart::Config& config)
    : Cart(config) {
    handle_video_setting();
    init();
  }

  virtual ~MgsCart() override {
    deinit();
  }

  // cppcheck-suppress uselessOverride
  virtual void reset() override {
    Cart::reset();
#if defined(ENABLE_MGS)
    reset_mgs();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void load() override {
    Cart::load();
#if defined(ENABLE_MGS)
    load_mgs(get_save_path(), get_selected_save_slot());
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void save() override {
    Cart::save();
#if defined(ENABLE_MGS)
    save_mgs(get_save_path(true), get_selected_save_slot());
#endif
  }

  void init() {
#if defined(ENABLE_MGS)
    init_mgs(get_rom_filename(), romdata_, rom_size_bytes_);
#endif
  }

  void deinit() {
#if defined(ENABLE_MGS)
    deinit_mgs();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual bool run() override {
#if defined(ENABLE_MGS)
    run_mgs_rom();
    if (mgs_quit_requested()) {
      running_ = false;
      return false;
    }
#endif
    return Cart::run();
  }

protected:
  // the PSX output the game is configured for; same as the box's panel
  static constexpr size_t MGS_WIDTH = 320;
  static constexpr size_t MGS_HEIGHT = 240;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_MGS)
    logger_.info("mgs::pre_menu()");
    pause_mgs_tasks();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_MGS)
    logger_.info("mgs::post_menu()");
    resume_mgs_tasks();
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_MGS)
    logger_.info("mgs::video: original");
    BoxEmu::get().display_size(MGS_WIDTH, MGS_HEIGHT);
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(MGS_WIDTH, MGS_HEIGHT);
  }

  // cppcheck-suppress uselessOverride
  virtual std::span<uint8_t> get_video_buffer() const override {
#if defined(ENABLE_MGS)
    return get_mgs_video_buffer();
#else
    return std::span<uint8_t>();
#endif
  }

  virtual void set_fit_video_setting() override {
#if defined(ENABLE_MGS)
    logger_.info("mgs::video: fit");
    BoxEmu::get().display_size(SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_MGS)
    logger_.info("mgs::video: fill");
    BoxEmu::get().display_size(SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
  }

  virtual std::string get_save_extension() const override {
    return "_mgs.sav";
  }
};
