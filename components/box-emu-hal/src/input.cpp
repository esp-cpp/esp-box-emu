#include "input.h"

#include "hal_i2c.hpp"

#include "task.hpp"

#include "mcp23x17.hpp"

#include "touchpad_input.hpp"
#include "tt21100.hpp"

using namespace std::chrono_literals;

static std::shared_ptr<espp::Mcp23x17> mcp23x17;
static std::shared_ptr<espp::Tt21100> tt21100;
static std::shared_ptr<espp::TouchpadInput> touchpad;

/**
 * Touch Controller configuration
 */
void touchpad_read(uint8_t* num_touch_points, uint16_t* x, uint16_t* y, uint8_t* btn_state) {
  // get the latest data from the device
  std::error_code ec;
  tt21100->update(ec);
  if (ec) {
    fmt::print("error updating tt21100: {}\n", ec.message());
    return;
  }
  // now hand it off
  tt21100->get_touch_point(num_touch_points, x, y);
  *btn_state = tt21100->get_home_button_state();
}

static std::atomic<bool> initialized = false;
void init_input() {
  if (initialized) return;
  fmt::print("Initializing input drivers...\n");

  fmt::print("Initializing Tt21100\n");
  tt21100 = std::make_shared<espp::Tt21100>(espp::Tt21100::Config{
      .read = std::bind(&espp::I2c::read, internal_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      .log_level = espp::Logger::Verbosity::WARN
    });

  fmt::print("Initializing touchpad\n");
  touchpad = std::make_shared<espp::TouchpadInput>(espp::TouchpadInput::Config{
      .touchpad_read = touchpad_read,
      .swap_xy = false,
      .invert_x = true,
      .invert_y = false,
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
  // d-pad, abxy = B0-B3, B4-B7
  auto b_pins = mcp23x17->get_pins(espp::Mcp23x17::Port::B, ec);
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
