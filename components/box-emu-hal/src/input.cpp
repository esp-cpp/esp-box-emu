#include <mutex>

#include "input.h"

#include "hal_i2c.hpp"

#include "mcp23x17.hpp"
#include "timer.hpp"
#include "touchpad_input.hpp"
#include "keypad_input.hpp"

using namespace std::chrono_literals;
using namespace box_hal;

struct TouchpadData {
  uint8_t num_touch_points = 0;
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t btn_state = 0;
};

static std::shared_ptr<espp::Mcp23x17> mcp23x17;
static std::shared_ptr<TouchDriver> touch_driver;
static std::shared_ptr<espp::TouchpadInput> touchpad;
static std::shared_ptr<espp::KeypadInput> keypad;
static std::shared_ptr<espp::Timer> input_timer;
static struct InputState gamepad_state;
static std::mutex gamepad_state_mutex;
static TouchpadData touchpad_data;
static std::mutex touchpad_data_mutex;

/**
 * Touch Controller configuration
 */
void touchpad_read(uint8_t* num_touch_points, uint16_t* x, uint16_t* y, uint8_t* btn_state) {
  std::lock_guard<std::mutex> lock(touchpad_data_mutex);
  *num_touch_points = touchpad_data.num_touch_points;
  *x = touchpad_data.x;
  *y = touchpad_data.y;
  *btn_state = touchpad_data.btn_state;
}

void keypad_read(bool *up, bool *down, bool *left, bool *right, bool *enter, bool *escape) {
  InputState state;
  get_input_state(&state);
  *up = state.up;
  *down = state.down;
  *left = state.left;
  *right = state.right;

  *enter = state.a;
  *escape = state.b;
}

void update_touchpad_input() {
  // get the latest data from the device
  std::error_code ec;
  bool new_data = touch_driver->update(ec);
  if (ec) {
    fmt::print("error updating touch_driver: {}\n", ec.message());
    std::lock_guard<std::mutex> lock(touchpad_data_mutex);
    touchpad_data = {};
    return;
  }
  if (!new_data) {
    std::lock_guard<std::mutex> lock(touchpad_data_mutex);
    touchpad_data = {};
    return;
  }
  // get the latest data from the touchpad
  TouchpadData temp_data;
  touch_driver->get_touch_point(&temp_data.num_touch_points, &temp_data.x, &temp_data.y);
  temp_data.btn_state = touch_driver->get_home_button_state();
  // update the touchpad data
  std::lock_guard<std::mutex> lock(touchpad_data_mutex);
  touchpad_data = temp_data;
}

void update_gamepad_input() {
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
  {
    std::lock_guard<std::mutex> lock(gamepad_state_mutex);
    gamepad_state.a = is_a_pressed;
    gamepad_state.b = is_b_pressed;
    gamepad_state.x = is_x_pressed;
    gamepad_state.y = is_y_pressed;
    gamepad_state.start = is_start_pressed;
    gamepad_state.select = is_select_pressed;
    gamepad_state.up = is_up_pressed;
    gamepad_state.down = is_down_pressed;
    gamepad_state.left = is_left_pressed;
    gamepad_state.right = is_right_pressed;
  }
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

  fmt::print("Initializing keypad\n");
  keypad = std::make_shared<espp::KeypadInput>(espp::KeypadInput::Config{
      .read = keypad_read,
      .log_level = espp::Logger::Verbosity::WARN
    });

  fmt::print("Initializing input task\n");
  input_timer = std::make_shared<espp::Timer>(espp::Timer::Config{
      .name = "Input timer",
      .period = 20ms,
      .callback = []() {
        update_touchpad_input();
        update_gamepad_input();
        return false;
      },
      .log_level = espp::Logger::Verbosity::WARN});

  initialized = true;
}

extern "C" lv_indev_t *get_keypad_input_device() {
  if (!keypad) {
    fmt::print("cannot get keypad input device: keypad not initialized properly!\n");
    return nullptr;
  }
  return keypad->get_input_device();
}

extern "C" void get_input_state(struct InputState* state) {
  std::lock_guard<std::mutex> lock(gamepad_state_mutex);
  *state = gamepad_state;
}
