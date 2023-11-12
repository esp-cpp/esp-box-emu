#pragma once

#include "cart.hpp"
#include "sms.hpp"

class SmsCart : public Cart {
public:

  SmsCart(const Cart::Config& config)
    : Cart(config) {
    init();
  }

  ~SmsCart() {
    deinit();
  }

  virtual void reset() override {
    Cart::reset();
    reset_sms();
  }

  virtual void load() override {
    Cart::load();
    load_sms(get_save_path());
  }

  virtual void save() override {
    Cart::save();
    save_sms(get_save_path(true));
  }

  virtual void init() override {
    Cart::init();
    init_sms(get_rom_filename(), romdata_, rom_size_bytes_);
    start_sms_tasks();
  }

  virtual void deinit() override {
    stop_sms_tasks();
    deinit_sms();
  }

  virtual bool run() override {
    run_sms_rom();
    return Cart::run();
  }

protected:
  // SMS
  static constexpr size_t SMS_WIDTH = 256;
  static constexpr size_t SMS_HEIGHT = 192;

  virtual void pre_menu() override {
    Cart::pre_menu();
    logger_.info("sms::pre_menu()");
    stop_sms_tasks();
  }

  virtual void post_menu() override {
    Cart::post_menu();
    logger_.info("sms::post_menu()");
    start_sms_tasks();
  }

  virtual void set_original_video_setting() override {
    logger_.info("sms::video: original");
    set_sms_video_original();
  }

  virtual std::pair<size_t, size_t> get_video_size() const override {
    return std::make_pair(SMS_WIDTH, SMS_HEIGHT);
  }

  virtual std::vector<uint8_t> get_video_buffer() const override {
    return get_sms_video_buffer();
  }

  virtual void set_fit_video_setting() override {
    logger_.info("sms::video: fit");
    set_sms_video_fit();
  }

  virtual void set_fill_video_setting() override {
    logger_.info("sms::video: fill");
    set_sms_video_fill();
  }

  virtual std::string get_save_extension() const override {
    switch (info_.platform) {
    case Emulator::SEGA_MASTER_SYSTEM:
      return "_sms.sav";
    default:
      return Cart::get_save_extension();
    }
  }
};
