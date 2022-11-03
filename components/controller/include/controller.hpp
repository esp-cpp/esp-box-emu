#pragma once

#include <mutex>

#include "driver/dedic_gpio.h"
#include "driver/gpio.h"

#include "logger.hpp"
#include "joystick.hpp"

/**
  * @brief Class for managing controller input.
  *
  * The controller can be configured to either use a digital d-pad or an analog
  * 2-axis joystick with select button.
  *
  * Digital configuration can support ABXY, start, select, and 4 digital
  * directional inputs.
  *
  * Anaolg Joystick Configuration can support ABXY, start, select, two axis
  * (analog) joystick, and joystick select button. It will also convert the
  * joystick analog values into digital d-pad buttons.
  */
class Controller {
public:
  enum class Button : int { A=0, B, X, Y, SELECT, START, UP, DOWN, LEFT, RIGHT, JOYSTICK_SELECT, LAST_UNUSED };
  struct State {
    uint32_t a : 1;
    uint32_t b : 1;
    uint32_t x : 1;
    uint32_t y : 1;
    uint32_t select : 1;
    uint32_t start : 1;
    uint32_t up : 1;
    uint32_t down : 1;
    uint32_t left : 1;
    uint32_t right : 1;
    uint32_t joystick_select : 1;
  };

  struct DigitalConfig {
    bool active_low{true};
    int gpio_a{-1};
    int gpio_b{-1};
    int gpio_x{-1};
    int gpio_y{-1};
    int gpio_start{-1};
    int gpio_select{-1};
    int gpio_up{-1};
    int gpio_down{-1};
    int gpio_left{-1};
    int gpio_right{-1};
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  struct AnalogJoystickConfig {
    bool active_low{true};
    int gpio_a{-1};
    int gpio_b{-1};
    int gpio_x{-1};
    int gpio_y{-1};
    int gpio_start{-1};
    int gpio_select{-1};
    int gpio_joystick_select{-1};
    espp::Joystick::Config joystick_config;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  Controller(const DigitalConfig& config)
    : logger_({.tag = "Digital Controller", .level = config.log_level}) {
    gpio_.assign((int)Button::LAST_UNUSED, -1);
    input_state_.assign((int)Button::LAST_UNUSED, false);
    gpio_[(int)Button::A] = config.gpio_a;
    gpio_[(int)Button::B] = config.gpio_b;
    gpio_[(int)Button::X] = config.gpio_x;
    gpio_[(int)Button::Y] = config.gpio_y;
    gpio_[(int)Button::START] = config.gpio_start;
    gpio_[(int)Button::SELECT] = config.gpio_select;
    gpio_[(int)Button::UP] = config.gpio_up;
    gpio_[(int)Button::DOWN] = config.gpio_down;
    gpio_[(int)Button::LEFT] = config.gpio_left;
    gpio_[(int)Button::RIGHT] = config.gpio_right;
    init_gpio(config.active_low);
  }

  Controller(const AnalogJoystickConfig& config)
    : joystick_(std::make_unique<espp::Joystick>(config.joystick_config)),
      logger_({.tag = "Analog Joystick Controller", .level = config.log_level}) {
    gpio_.assign((int)Button::LAST_UNUSED, -1);
    input_state_.assign((int)Button::LAST_UNUSED, false);
    gpio_[(int)Button::A] = config.gpio_a;
    gpio_[(int)Button::B] = config.gpio_b;
    gpio_[(int)Button::X] = config.gpio_x;
    gpio_[(int)Button::Y] = config.gpio_y;
    gpio_[(int)Button::START] = config.gpio_start;
    gpio_[(int)Button::SELECT] = config.gpio_select;
    gpio_[(int)Button::JOYSTICK_SELECT] = config.gpio_joystick_select;
    init_gpio(config.active_low);
  }

  ~Controller() {
    dedic_gpio_del_bundle(gpio_bundle_);
  }

  State get_state() {
    logger_.debug("Returning state structure");
    std::scoped_lock<std::mutex> lk(state_mutex_);
    return State {
      .a = input_state_[(int)Button::A],
      .b = input_state_[(int)Button::B],
      .x = input_state_[(int)Button::X],
      .y = input_state_[(int)Button::Y],
      .select = input_state_[(int)Button::SELECT],
      .start = input_state_[(int)Button::START],
      .up = input_state_[(int)Button::UP],
      .down = input_state_[(int)Button::DOWN],
      .left = input_state_[(int)Button::LEFT],
      .right = input_state_[(int)Button::RIGHT],
      .joystick_select = input_state_[(int)Button::JOYSTICK_SELECT],
    };
  }

  bool is_pressed(const Button input) {
    std::scoped_lock<std::mutex> lk(state_mutex_);
    return input_state_[(int)input];
  }

  void update() {
    logger_.debug("Reading gpio bundle");
    // read the updated state for configured gpios (all at once)
    uint32_t pin_state = dedic_gpio_bundle_read_in(gpio_bundle_);
    // when setting up the dedic gpio, we removed the gpio that were not
    // configured (-1) in our vector, but the returned bitmask simply orders
    // them (low bit is low member in originally provided vector) so we need to
    // track the actual bit corresponding to the pin in the pin_state.
    int bit = 0;
    // and pull out the state into the vector accordingly
    logger_.debug("Parsing bundle state from pin state 0x{:04X}", pin_state);
    {
      std::scoped_lock<std::mutex> lk(state_mutex_);
      for (int i=0; i<gpio_.size(); i++) {
        auto gpio = gpio_[i];
        if (gpio != -1) {
          input_state_[i] = is_bit_set(pin_state, bit);
          // this pin is used, increment the bit index
          bit++;
        } else {
          input_state_[i] = false;
        }
      }
    }
    // now update the joystick if we have it
    if (joystick_) {
      logger_.debug("Updating joystick");
      joystick_->update();
      // now update the d-pad state if the joystick values are high enough
      float x = joystick_->x();
      float y = joystick_->y();
      logger_.debug("Got joystick x,y: ({},{})", x, y);
      std::scoped_lock<std::mutex> lk(state_mutex_);
      if (x > 0.5f) {
        input_state_[(int)Button::RIGHT] = true;
      } else if (x < -0.5f) {
        input_state_[(int)Button::LEFT] = true;
      }
      if (y > 0.5f) {
        input_state_[(int)Button::UP] = true;
      } else if (y < -0.5f) {
        input_state_[(int)Button::DOWN] = true;
      }
    }
  }

protected:
  bool is_bit_set(uint32_t data, int bit) {
    return (data & (1 << bit)) != 0;
  }

  void init_gpio(bool active_low) {
    // select only the gpios that are used (not -1)
    std::vector<int> actual_gpios;
    for (auto gpio : gpio_) {
      if (gpio != -1) {
        actual_gpios.push_back(gpio);
      }
    }

    uint64_t pin_mask = 0;
    for (auto gpio : actual_gpios) {
      pin_mask |= (1ULL << gpio);
    }
    gpio_config_t io_config = {
      .pin_bit_mask = pin_mask,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
      .pull_down_en = active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    // Create gpio_bundle_, input only
    dedic_gpio_bundle_config_t gpio_bundle_config = {
      .gpio_array = actual_gpios.data(),
      .array_size = actual_gpios.size(),
      .flags = {
        .in_en = 1,
        .in_invert = (unsigned int)(active_low ? 1 : 0),
        .out_en = 0, // we _could_ enable input & output but we don't want to
        .out_invert = 0,
      },
    };
    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&gpio_bundle_config, &gpio_bundle_));
  }

  std::mutex state_mutex_;
  std::vector<int> gpio_;
  std::vector<bool> input_state_;
  dedic_gpio_bundle_handle_t gpio_bundle_{NULL};
  std::unique_ptr<espp::Joystick> joystick_;
  espp::Logger logger_;
};
