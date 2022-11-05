#pragma once

#include <functional>

#include "logger.hpp"

class Tt21100 {
public:
  static constexpr uint8_t ADDRESS = (0x24);

  typedef std::function<void(uint8_t*, size_t)> read_fn;

  struct Config {
    read_fn read;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN}; /**< Log verbosity for the input driver.  */
  };

  Tt21100(const Config& config)
    : read_(config.read),
      logger_({.tag = "Tt21100", .level = config.log_level}) {
    init();
  }

  bool read() {
    static uint16_t data_len;
    static uint8_t data[256];

    read_((uint8_t*)&data_len, sizeof(data_len));
    logger_.debug("Data length: {}", data_len);

    if (data_len == 0xff) {
      return false;
    }

    read_(data, data_len);
    switch (data_len) {
    case 2:
      // no available data
      break;
    case 7:
    case 17:
    case 27: {
      // touch event - NOTE: this only gets the first touch record
      auto report_data = (TouchReport*) data;
      auto touch_data = (TouchRecord*)(&report_data->touch_record[0]);
      x_ = touch_data->x;
      y_ = touch_data->y;
      num_touch_points_ = (data_len - sizeof(TouchReport)) / sizeof(TouchRecord);
      logger_.debug("Touch event: #={}, [0]=({}, {})", num_touch_points_, x_, y_);
      break;
    }
    case 14: {
      // button event
      auto button_data = (ButtonRecord*)data;
      home_button_pressed_ = button_data->btn_val;
      auto btn_signal = button_data->btn_signal[0];
      logger_.debug("Button event({}): {}, {}", (int)(button_data->length), home_button_pressed_, btn_signal);
      break;
    }
    default:
      break;
    }
    return true;
  }

  uint8_t get_num_touch_points() {
    return num_touch_points_;
  }

  void get_touch_point(uint8_t *num_touch_points, uint16_t *x, uint16_t *y) {
    *num_touch_points = get_num_touch_points();
    if (*num_touch_points != 0) {
      *x = x_;
      *y = y_;
      logger_.info("Got touch ({}, {})", *x, *y);
    }
  }

  uint8_t get_home_button_state() {
    return home_button_pressed_;
  }

protected:
  void init() {
    uint16_t reg_val = 0;
    do {
      using namespace std::chrono_literals;
      read_((uint8_t*)&reg_val, sizeof(reg_val));
      std::this_thread::sleep_for(20ms);
    } while (0x0002 != reg_val);
  }

  enum class Registers : uint8_t {
    TP_NUM = 0x01,
    X_POS  = 0x02,
    Y_POS  = 0x03,
  };

  struct TouchRecord {
  uint8_t :5;
    uint8_t touch_type:3;
    uint8_t tip:1;
    uint8_t event_id:2;
    uint8_t touch_id:5;
    uint16_t x;
    uint16_t y;
    uint8_t pressure;
    uint16_t major_axis_length;
    uint8_t orientation;
  } __attribute__((packed));

  struct TouchReport {
    uint16_t data_len;
    uint8_t report_id;
    uint16_t time_stamp;
  uint8_t :2;
    uint8_t large_object : 1;
    uint8_t record_num : 5;
    uint8_t report_counter:2;
  uint8_t :3;
    uint8_t noise_efect:3;
    TouchRecord touch_record[0];
  } __attribute__((packed));

  struct ButtonRecord {
    uint16_t length;        /*!< Always 14(0x000E) */
    uint8_t report_id;      /*!< Always 03h */
    uint16_t time_stamp;    /*!< Number in units of 100 us */
    uint8_t btn_val;        /*!< Only use bit[0..3] */
    uint16_t btn_signal[4];
  } __attribute__((packed));

  read_fn read_;
  std::atomic<bool> home_button_pressed_{false};
  std::atomic<uint8_t> num_touch_points_;
  std::atomic<uint16_t> x_;
  std::atomic<uint16_t> y_;
  espp::Logger logger_;
};
