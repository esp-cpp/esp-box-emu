#include "box-emu.hpp"

void BoxEmu::detect() {
  bool mcp23x17_found = external_i2c_.probe_device(espp::Mcp23x17::DEFAULT_ADDRESS);
  bool aw9523_found = external_i2c_.probe_device(espp::Aw9523::DEFAULT_ADDRESS);
  if (aw9523_found) {
    // Version 1
    version_ = BoxEmu::Version::V1;
  } else if (mcp23x17_found) {
    // Version 0
    version_ = BoxEmu::Version::V0;
  } else {
    logger_.warn("No box detected");
    // No box detected
    version_ = BoxEmu::Version::UNKNOWN;
    return;
  }
  logger_.info("version {}", version_);
}

extern "C" lv_indev_t *get_keypad_input_device() {
  auto keypad = espp::EspBox::get().keypad();
  if (!keypad) {
    fmt::print("cannot get keypad input device: keypad not initialized properly!\n");
    return nullptr;
  }
  return keypad->get_input_device();
}

bool BoxEmu::initialize_gamepad() {
  if (version_ == BoxEmu::Version::V0) {
    auto raw_input = new Input<version0, version0::InputDriver>(
                                                                std::make_shared<version0::InputDriver>(version0::InputDriver::Config{
                                                                    .port_0_direction_mask = version0::PORT_0_DIRECTION_MASK,
                                                                    .port_0_interrupt_mask = version0::PORT_0_INTERRUPT_MASK,
                                                                    .port_1_direction_mask = version0::PORT_1_DIRECTION_MASK,
                                                                    .port_1_interrupt_mask = version0::PORT_1_INTERRUPT_MASK,
                                                                    .write = std::bind(&espp::I2c::write, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                                                                    .write_then_read = std::bind(&espp::I2c::write_read, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5),
                                                                    .log_level = espp::Logger::Verbosity::WARN
                                                                  })
                                                                );
    input_.reset(raw_input);
  } else if (version_ == BoxEmu::Version::V1) {
    auto raw_input = new Input<version0, version0::InputDriver>(
                                                                std::make_shared<version1::InputDriver>(version1::InputDriver::Config{
                                                                    .port_0_direction_mask = version1::PORT_0_DIRECTION_MASK,
                                                                    .port_0_interrupt_mask = version1::PORT_0_INTERRUPT_MASK,
                                                                    .port_1_direction_mask = version1::PORT_1_DIRECTION_MASK,
                                                                    .port_1_interrupt_mask = version1::PORT_1_INTERRUPT_MASK,
                                                                    .write = std::bind(&espp::I2c::write, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                                                                    .write_then_read = std::bind(&espp::I2c::write_read, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5),
                                                                    .log_level = espp::Logger::Verbosity::WARN
                                                                  })
                                                                );
    input_.reset(raw_input);
  } else {
    return false;
  }

  // now initialize the keypad driver
  keypad_ = std::make_shared<espp::KeypadInput>(espp::KeypadInput::Config{
      .read = keypad_read,
      .log_level = espp::Logger::Verbosity::WARN
    });

  // now initialize the input timer
  input_timer_ = std::make_shared<espp::HighResolutionTimer>(espp::HighResolutionTimer::Config{
      .name = "Input timer",
      .callback = []() {
        espp::EspBox::get().update_touch();
        update_gamepad_state();
      }});
  uint64_t period_us = 30 * 1000;
  input_timer_->periodic(period_us);

  return true;
}

InputState BoxEmu::gamepad_state() {
  std::lock_guard<std::mutex> lock(input_mutex_);
  return input_state_;
}

bool BoxEmu::update_gamepad_state() {
  if (!input_) {
    return false;
  }
  if (!can_read_gamepad_) {
    return false;
  }
  std::error_code ec;
  auto pins = input_->get_pins(ec);
  if (ec) {
    logger_.error("Error reading input pins: {}", ec.message());
    can_read_gamepad_ = false;
    return false;
  }

  auto new_input_state = input_->pins_to_gamepad_state(pins);
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(input_mutex_);
    changed = input_state_ != new_input_state;
    input_state_ = new_input_state;
  }
  input_->handle_volume_pins(pins);

  return changed;
}

void BoxEmu::keypad_read(bool *up, bool *down, bool *left, bool *right, bool *enter, bool *escape) {
  InputState state;
  hal::get_input_state(&state);
  *up = state.up;
  *down = state.down;
  *left = state.left;
  *right = state.right;

  *enter = state.a || state.start;
  *escape = state.b || state.select;
}
