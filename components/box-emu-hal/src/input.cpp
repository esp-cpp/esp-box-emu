#include "input.h"

#include "hal_i2c.hpp"

#include "task.hpp"

#include "mcp23x17.hpp"

#include "touchpad_input.hpp"

using namespace std::chrono_literals;
using namespace box_hal;

static std::shared_ptr<espp::Mcp23x17> mcp23x17;
static std::shared_ptr<TouchDriver> touch_driver;
static std::shared_ptr<espp::TouchpadInput> touchpad;

/**
 * Touch Controller configuration
 */
void touchpad_read(uint8_t* num_touch_points, uint16_t* x, uint16_t* y, uint8_t* btn_state) {
  *num_touch_points = 0;
  // get the latest data from the device
  std::error_code ec;
  bool new_data = touch_driver->update(ec);
  if (ec) {
    fmt::print("error updating touch_driver: {}\n", ec.message());
    return;
  }
  if (!new_data) {
    return;
  }
  // now hand it off
  touch_driver->get_touch_point(num_touch_points, x, y);
  *btn_state = touch_driver->get_home_button_state();
}

static std::atomic<bool> initialized = false;
void init_input() {
  if (initialized) return;
  fmt::print("Initializing input drivers...\n");

  fmt::print("Initializing touch driver\n");
  touch_driver = std::make_shared<TouchDriver>(TouchDriver::Config{
#if TOUCH_DRIVER_USE_WRITE
      .write = std::bind(&espp::I2c::write, internal_i2c.get(), std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3),
#endif
#if TOUCH_DRIVER_USE_READ
      .read = std::bind(&espp::I2c::read, internal_i2c.get(), std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3),
#endif
#if TOUCH_DRIVER_USE_WRITE_READ
      .write_read = std::bind(&espp::I2c::write_read, internal_i2c.get(), std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3,
                              std::placeholders::_4, std::placeholders::_5),
#endif
      .log_level = espp::Logger::Verbosity::WARN
    });

  fmt::print("Initializing touchpad\n");
  touchpad = std::make_shared<espp::TouchpadInput>(espp::TouchpadInput::Config{
      .touchpad_read = touchpad_read,
      .swap_xy = touch_swap_xy,
      .invert_x = touch_invert_x,
      .invert_y = touch_invert_y,
      .log_level = espp::Logger::Verbosity::WARN
    });

  fmt::print("initializing MCP23X17\n");
  mcp23x17 = std::make_shared<espp::Mcp23x17>(espp::Mcp23x17::Config{
      .port_a_direction_mask = 0x03,   // Start on A0, Select on A1
      .port_a_interrupt_mask = 0x00,
      .port_b_direction_mask = 0xFF,   // D-pad B0-B3, ABXY B4-B7
      .port_b_interrupt_mask = 0x00,
      .write = std::bind(&espp::I2c::write, external_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      .read = std::bind(&espp::I2c::read_at_register, external_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
      .log_level = espp::Logger::Verbosity::WARN
    });

  initialized = true;
}

extern "C" void get_input_state(struct InputState* state) {
  bool is_a_pressed = false;
  bool is_b_pressed = false;
  bool is_x_pressed = false;
  bool is_y_pressed = false;
  bool is_select_pressed = false;
  bool is_start_pressed = false;
  bool is_up_pressed = false;
  bool is_down_pressed = false;
  bool is_left_pressed = false;
  bool is_right_pressed = false;
  if (!mcp23x17) {
    fmt::print("cannot get input state: mcp23x17 not initialized properly!\n");
    return;
  }
  // pins are active low
  // start, select = A0, A1
  std::error_code ec;
  auto a_pins = mcp23x17->get_pins(espp::Mcp23x17::Port::A, ec);
  if (ec) {
    fmt::print("error getting pins from mcp23x17: {}\n", ec.message());
    return;
  }
  // d-pad, abxy = B0-B3, B4-B7
  auto b_pins = mcp23x17->get_pins(espp::Mcp23x17::Port::B, ec);
  if (ec) {
    fmt::print("error getting pins from mcp23x17: {}\n", ec.message());
    return;
  }
  is_a_pressed = !(b_pins & 1<<4);
  is_b_pressed = !(b_pins & 1<<5);
  is_x_pressed = !(b_pins & 1<<6);
  is_y_pressed = !(b_pins & 1<<7);
  is_start_pressed = !(a_pins & 1<<0);
  is_select_pressed = !(a_pins & 1<<1);
  is_up_pressed = !(b_pins & 1<<0);
  is_down_pressed = !(b_pins & 1<<1);
  is_left_pressed = !(b_pins & 1<<2);
  is_right_pressed = !(b_pins & 1<<3);
  state->a = is_a_pressed;
  state->b = is_b_pressed;
  state->x = is_x_pressed;
  state->y = is_y_pressed;
  state->start = is_start_pressed;
  state->select = is_select_pressed;
  state->up = is_up_pressed;
  state->down = is_down_pressed;
  state->left = is_left_pressed;
  state->right = is_right_pressed;
}
