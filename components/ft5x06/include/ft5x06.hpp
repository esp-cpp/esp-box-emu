#pragma once

#include <functional>

#include "logger.hpp"

class Ft5x06 {
public:
  static constexpr uint8_t ADDRESS = (0x38);

  typedef std::function<void(uint8_t, uint8_t)> write_fn;
  typedef std::function<void(uint8_t, uint8_t*, size_t)> read_fn;

  enum class Gesture : uint8_t {
    NONE         = 0x00,
    MOVE_UP      = 0x10,
    MOVE_LEFT    = 0x14,
    MOVE_DOWN    = 0x18,
    MOVE_RIGHT   = 0x1C,
    ZOOM_IN      = 0x48,
    ZOOM_OUT     = 0x49,
  };

  struct Config {
    write_fn write;
    read_fn read;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN}; /**< Log verbosity for the input driver.  */
  };

  Ft5x06(const Config& config)
    : write_(config.write),
      read_(config.read),
      logger_({.tag = "Ft5x06", .level = config.log_level}) {
    init();
  }

  uint8_t get_num_touch_points() {
    uint8_t num_touch_points;
    read_((uint8_t)Registers::TOUCH_POINTS, &num_touch_points, 1);
    logger_.info("Got num touch points {}", num_touch_points);
    return num_touch_points;
  }

  void get_touch_point(uint8_t *num_touch_points, uint16_t *x, uint16_t *y) {
    *num_touch_points = get_num_touch_points();
    if (*num_touch_points != 0) {
      uint8_t data[4];
      read_((uint8_t)Registers::TOUCH1_XH, (uint8_t*)data, 4);
      *x = ((data[0] & 0x0f) << 8) + data[1];
      *y = ((data[2] & 0x0f) << 8) + data[3];
      logger_.info("Got touch ({}, {})", *x, *y);
    }
  }

  Gesture read_gesture() {
    uint8_t data;
    read_((uint8_t)Registers::GESTURE_ID, &data, 1);
    logger_.info("Got gesture {}", data);
    return (Gesture)data;
  }

protected:
  void init() {
    // Valid touching detect threshold
    write_((uint8_t)Registers::ID_G_THGROUP, 70);
    // valid touching peak detect threshold
    write_((uint8_t)Registers::ID_G_THPEAK, 60);
    // Touch focus threshold
    write_((uint8_t)Registers::ID_G_THCAL, 16);
    // threshold when there is surface water
    write_((uint8_t)Registers::ID_G_THWATER, 60);
    // threshold of temperature compensation
    write_((uint8_t)Registers::ID_G_THTEMP, 10);
    // Touch difference threshold
    write_((uint8_t)Registers::ID_G_THDIFF, 20);
    // Delay to enter 'Monitor' status (s)
    write_((uint8_t)Registers::ID_G_TIME_ENTER_MONITOR, 2);
    // Period of 'Active' status (ms)
    write_((uint8_t)Registers::ID_G_PERIODACTIVE, 12);
    // Timer to enter 'idle' when in 'Monitor' (ms)
    write_((uint8_t)Registers::ID_G_PERIODMONITOR, 40);
  }

  enum class Registers : uint8_t {
    DEVICE_MODE      = 0x00,
    GESTURE_ID       = 0x01,
    TOUCH_POINTS     = 0x02,

    TOUCH1_EV_FLAG   = 0x03,
    TOUCH1_XH        = 0x03,
    TOUCH1_XL        = 0x04,
    TOUCH1_YH        = 0x05,
    TOUCH1_YL        = 0x06,

    TOUCH2_EV_FLAG   = 0x09,
    TOUCH2_XH        = 0x09,
    TOUCH2_XL        = 0x0A,
    TOUCH2_YH        = 0x0B,
    TOUCH2_YL        = 0x0C,

    TOUCH3_EV_FLAG   = 0x0F,
    TOUCH3_XH        = 0x0F,
    TOUCH3_XL        = 0x10,
    TOUCH3_YH        = 0x11,
    TOUCH3_YL        = 0x12,

    TOUCH4_EV_FLAG   = 0x15,
    TOUCH4_XH        = 0x15,
    TOUCH4_XL        = 0x16,
    TOUCH4_YH        = 0x17,
    TOUCH4_YL        = 0x18,

    TOUCH5_EV_FLAG   = 0x1B,
    TOUCH5_XH        = 0x1B,
    TOUCH5_XL        = 0x1C,
    TOUCH5_YH        = 0x1D,
    TOUCH5_YL        = 0x1E,

    ID_G_THGROUP             = 0x80,
    ID_G_THPEAK              = 0x81,
    ID_G_THCAL               = 0x82,
    ID_G_THWATER             = 0x83,
    ID_G_THTEMP              = 0x84,
    ID_G_THDIFF              = 0x85,
    ID_G_CTRL                = 0x86,
    ID_G_TIME_ENTER_MONITOR  = 0x87,
    ID_G_PERIODACTIVE        = 0x88,
    ID_G_PERIODMONITOR       = 0x89,
    ID_G_AUTO_CLB_MODE       = 0xA0,
    ID_G_LIB_VERSION_H       = 0xA1,
    ID_G_LIB_VERSION_L       = 0xA2,
    ID_G_CIPHER              = 0xA3,
    ID_G_MODE                = 0xA4,
    ID_G_PMODE               = 0xA5,
    ID_G_FIRMID              = 0xA6,
    ID_G_STATE               = 0xA7,
    ID_G_FT5201ID            = 0xA8,
    ID_G_ERR                 = 0xA9,
  };

  write_fn write_;
  read_fn read_;
  espp::Logger logger_;
};
