#pragma once

#include <functional>

#include "logger.hpp"

class QwiicNes {
public:
  static constexpr uint8_t ADDRESS = (0x54);

  typedef std::function<void(uint8_t, uint8_t)> write_fn;
  typedef std::function<uint8_t(uint8_t)> read_fn;

  /**
   * @brief The buttons on the NES controller. The values in this enum match the
   *        button's corresponding bit field in the byte returned by
   *        read_current_state() and read_buton_accumulator().
   */
  enum class Button : int {
    A, B, SELECT, START, UP, DOWN, LEFT, RIGHT
  };

  struct Config {
    write_fn write;
    read_fn read;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  QwiicNes(const Config& config)
    : write_(config.write),
      read_(config.read),
      logger_({.tag = "QwiicNes", .level = config.log_level}) {
  }

  bool is_pressed(Button button) const {
    int bit = (int)button;
    return accumulated_states_ & (1 << bit);
  }

  void update() {
    auto buttons = read_button_accumulator();
    logger_.info("updated state: {:02X}", buttons);
  }

  uint8_t read_current_state() {
    return read_((uint8_t)Registers::CURRENT_STATE);
  }

  uint8_t read_button_accumulator() {
    accumulated_states_ = read_((uint8_t)Registers::ACCUMULATOR);
    return accumulated_states_;
  }

protected:
  enum class Registers : uint8_t {
    CURRENT_STATE  = 0x00,
    ACCUMULATOR    = 0x01,
    ADDRESS        = 0x02,
    CHANGE_ADDRESS = 0x03,
  };

  write_fn write_;
  read_fn read_;
  uint8_t accumulated_states_{0};
  espp::Logger logger_;
};
