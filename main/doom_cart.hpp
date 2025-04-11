#pragma once

#include "cart.hpp"
#if defined(ENABLE_DOOM)
#include "doom.hpp"
#endif

class DoomCart : public Cart {
public:
  explicit DoomCart(const Cart::Config& config)
    : Cart(config) {
    handle_video_setting();
    init();
  }

  virtual ~DoomCart() override {
    logger_.info("~DoomCart()");
    deinit();
  }

  // cppcheck-suppress uselessOverride
  virtual void reset() override {
    Cart::reset();
#if defined(ENABLE_DOOM)
    // Doom doesn't have a reset function, we'll just reinitialize
    deinit();
    init();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void load() override {
    Cart::load();
#if defined(ENABLE_DOOM)
    load_doom(get_save_path());
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void save() override {
    Cart::save();
#if defined(ENABLE_DOOM)
    save_doom(get_save_path(true));
#endif
  }

  void init() {
#if defined(ENABLE_DOOM)
    logger_.info("doom::init()");
    init_doom(get_rom_filename(), romdata_, rom_size_bytes_);
#endif
  }

  void deinit() {
#if defined(ENABLE_DOOM)
    deinit_doom();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual bool run() override {
#if defined(ENABLE_DOOM)
    run_doom_rom();
#endif
    return Cart::run();
  }

protected:
  // DOOM
  static constexpr size_t DOOM_WIDTH = 320;
  static constexpr size_t DOOM_HEIGHT = 200;

  // cppcheck-suppress uselessOverride
  virtual void pre_menu() override {
    Cart::pre_menu();
#if defined(ENABLE_DOOM)
    logger_.info("doom::pre_menu()");
    stop_doom_tasks();
#endif
  }

  // cppcheck-suppress uselessOverride
  virtual void post_menu() override {
    Cart::post_menu();
#if defined(ENABLE_DOOM)
    logger_.info("doom::post_menu()");
    start_doom_tasks();
#endif
  }

  virtual void set_original_video_setting() override {
#if defined(ENABLE_DOOM)
    logger_.info("doom::video: original");
    set_doom_video_original();
#endif
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(DOOM_WIDTH, DOOM_HEIGHT);
  }

  // cppcheck-suppress uselessOverride
  virtual std::vector<uint8_t> get_video_buffer() const override {
#if defined(ENABLE_DOOM)
    return get_doom_video_buffer();
#else
    return std::vector<uint8_t>();
#endif
  }

  virtual void set_fit_video_setting() override {
#if defined(ENABLE_DOOM)
    logger_.info("doom::video: fit");
    set_doom_video_fit();
#endif
  }

  virtual void set_fill_video_setting() override {
#if defined(ENABLE_DOOM)
    logger_.info("doom::video: fill");
    set_doom_video_fill();
#endif
  }

  virtual std::string get_save_extension() const override {
    return "_doom.sav";
  }
}; 