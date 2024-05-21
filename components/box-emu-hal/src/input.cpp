
#include "box_emu_hal.hpp"

using namespace std::chrono_literals;

struct TouchpadData {
  uint8_t num_touch_points = 0;
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t btn_state = 0;
};

class InputBase {
public:
  virtual uint16_t get_pins(std::error_code& ec) = 0;
  virtual InputState pins_to_gamepad_state(uint16_t pins) = 0;
  virtual void handle_volume_pins(uint16_t pins) = 0;
};

template <typename T, typename InputDriver>
class Input : public InputBase {
public:
  explicit Input(std::shared_ptr<InputDriver> input_driver) : input_driver(input_driver) {}
  virtual uint16_t get_pins(std::error_code& ec) override {
    auto val = input_driver->get_pins(ec);
    if (ec) {
      return 0;
    }
    return val ^ T::INVERT_MASK;
  }
  virtual InputState pins_to_gamepad_state(uint16_t pins) override {
    InputState state;
    state.a = (bool)(pins & T::A_PIN);
    state.b = (bool)(pins & T::B_PIN);
    state.x = (bool)(pins & T::X_PIN);
    state.y = (bool)(pins & T::Y_PIN);
    state.start = (bool)(pins & T::START_PIN);
    state.select = (bool)(pins & T::SELECT_PIN);
    state.up = (bool)(pins & T::UP_PIN);
    state.down = (bool)(pins & T::DOWN_PIN);
    state.left = (bool)(pins & T::LEFT_PIN);
    state.right = (bool)(pins & T::RIGHT_PIN);
    return state;
  }
  virtual void handle_volume_pins(uint16_t pins) override {
    // check the volume pins and send out events if they're pressed / released
    bool volume_up = (bool)(pins & T::VOL_UP_PIN);
    bool volume_down = (bool)(pins & T::VOL_DOWN_PIN);
    int volume_change = (volume_up * 10) + (volume_down * -10);
    if (volume_change != 0) {
      // change the volume
      int current_volume = hal::get_audio_volume();
      int new_volume = std::clamp<int>(current_volume + volume_change, 0, 100);
      hal::set_audio_volume(new_volume);
      // send out a volume change event
      espp::EventManager::get().publish(volume_changed_topic, {});
    }
  }
protected:
  std::shared_ptr<InputDriver> input_driver;
};

static std::shared_ptr<InputBase> input;

static std::shared_ptr<TouchDriver> touch_driver;
static std::shared_ptr<espp::TouchpadInput> touchpad;
static std::shared_ptr<espp::KeypadInput> keypad;
static std::shared_ptr<espp::HighResolutionTimer> input_timer;
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
  hal::get_input_state(&state);
  *up = state.up;
  *down = state.down;
  *left = state.left;
  *right = state.right;

  *enter = state.a || state.start;
  *escape = state.b || state.select;
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
  static bool can_read_input = true;
  if (!input) {
    return;
  }
  if (!can_read_input) {
    return;
  }
  // pins are active low
  // start, select = A0, A1
  std::error_code ec;
  auto pins = input->get_pins(ec);
  if (ec) {
    fmt::print("error getting pins: {}\n", ec.message());
    can_read_input = false;
    return;
  }

  auto new_gamepad_state = input->pins_to_gamepad_state(pins);
  {
    std::lock_guard<std::mutex> lock(gamepad_state_mutex);
    gamepad_state = new_gamepad_state;
  }
  input->handle_volume_pins(pins);
  // TODO: check the battery alert pin and if it's low, send out a battery alert event
}

static void init_input_generic() {
  auto internal_i2c = hal::get_internal_i2c();

  fmt::print("Initializing touch driver\n");
  touch_driver = std::make_shared<TouchDriver>(TouchDriver::Config{
      .write = std::bind(&espp::I2c::write, internal_i2c.get(), std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3),
      .read = std::bind(&espp::I2c::read, internal_i2c.get(), std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3),
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

  auto external_i2c = hal::get_external_i2c();

  fmt::print("Initializing keypad\n");
  keypad = std::make_shared<espp::KeypadInput>(espp::KeypadInput::Config{
      .read = keypad_read,
      .log_level = espp::Logger::Verbosity::WARN
    });

  fmt::print("Initializing input task\n");
  input_timer = std::make_shared<espp::HighResolutionTimer>(espp::HighResolutionTimer::Config{
      .name = "Input timer",
      .callback = []() {
        update_touchpad_input();
        update_gamepad_input();
      }});
  uint64_t period_us = 20 * 1000;
  input_timer->periodic(period_us);
}

static void init_input_v0() {
  auto external_i2c = hal::get_external_i2c();
  fmt::print("initializing input driver\n");
  using InputDriver = espp::Mcp23x17;
  auto raw_input = new Input<EmuV0, InputDriver>(
      std::make_shared<InputDriver>(InputDriver::Config{
          .port_0_direction_mask = EmuV0::PORT_0_DIRECTION_MASK,
          .port_0_interrupt_mask = EmuV0::PORT_0_INTERRUPT_MASK,
          .port_1_direction_mask = EmuV0::PORT_1_DIRECTION_MASK,
          .port_1_interrupt_mask = EmuV0::PORT_1_INTERRUPT_MASK,
          .write = std::bind(&espp::I2c::write, external_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          .read_register = std::bind(&espp::I2c::read_at_register, external_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
          .log_level = espp::Logger::Verbosity::WARN
        }));
  input.reset(raw_input);
}

static void init_input_v1() {
  auto external_i2c = hal::get_external_i2c();
  fmt::print("initializing input driver\n");
  using InputDriver = espp::Aw9523;
  auto raw_input = new Input<EmuV1, InputDriver>(
      std::make_shared<InputDriver>(InputDriver::Config{
          .port_0_direction_mask = EmuV1::PORT_0_DIRECTION_MASK,
          .port_0_interrupt_mask = EmuV1::PORT_0_INTERRUPT_MASK,
          .port_1_direction_mask = EmuV1::PORT_1_DIRECTION_MASK,
          .port_1_interrupt_mask = EmuV1::PORT_1_INTERRUPT_MASK,
          .write = std::bind(&espp::I2c::write, external_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          .write_then_read = std::bind(&espp::I2c::write_read, external_i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5),
          .log_level = espp::Logger::Verbosity::WARN
        }));
  input.reset(raw_input);
}

static std::atomic<bool> initialized = false;
void hal::init_input() {
  if (initialized) return;
  fmt::print("Initializing input subsystem\n");
  // probe the i2c bus for the mcp23x17 (which would be v0) or the aw9523 (which
  // would be v1)
  auto i2c = hal::get_external_i2c();
  bool mcp23x17_found = i2c->probe_device(espp::Mcp23x17::DEFAULT_ADDRESS);
  bool aw9523_found = i2c->probe_device(espp::Aw9523::DEFAULT_ADDRESS);

  if (mcp23x17_found) {
    fmt::print("Found MCP23x17, initializing input VERSION 0\n");
    init_input_v0();
  } else if (aw9523_found) {
    fmt::print("Found AW9523, initializing input VERSION 1\n");
    init_input_v1();
  } else {
    fmt::print("ERROR: No input systems found!\n");
  }

  // now initialize the rest of the input systems which are common to both
  // versions
  init_input_generic();

  initialized = true;
}

extern "C" lv_indev_t *get_keypad_input_device() {
  if (!keypad) {
    fmt::print("cannot get keypad input device: keypad not initialized properly!\n");
    return nullptr;
  }
  return keypad->get_input_device();
}

void hal::get_input_state(struct InputState* state) {
  std::lock_guard<std::mutex> lock(gamepad_state_mutex);
  *state = gamepad_state;
}
