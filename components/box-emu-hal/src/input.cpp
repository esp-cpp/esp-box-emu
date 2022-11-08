#include "input.h"

#include "driver/i2c.h"

#include "task.hpp"

#define USE_QWIICNES 1
#if USE_QWIICNES
#include "qwiicnes.hpp"
#else
#include "controller.hpp"
#include "ads1x15.hpp"
#endif

#include "touchpad_input.hpp"
#define USE_FT5X06 0
#if USE_FT5X06
#include "ft5x06.hpp"
#else
#include "tt21100.hpp"
#endif

using namespace std::chrono_literals;

/* I2C port and GPIOs */
#define GAMEPAD_I2C_NUM (I2C_NUM_1)  // for the gamepad, this is external so we have to initialize it
#define TP_I2C_NUM      (I2C_NUM_0)  // for the touchpad, this is internal, so we don't have to initialize it
#define I2C_SCL_IO      (GPIO_NUM_40)
#define I2C_SDA_IO      (GPIO_NUM_41)
#define I2C_FREQ_HZ     (400 * 1000)
#define I2C_TIMEOUT_MS         1000
#define I2C_MASTER_TIMEOUT_MS (10)

static i2c_config_t i2c_cfg;
#if USE_QWIICNES
static std::shared_ptr<QwiicNes> qwiicnes;
#else
static std::shared_ptr<Controller> controller;
static std::shared_ptr<Ads1x15> ads;
static std::unique_ptr<espp::Task> ads_task;
#endif

#if USE_FT5X06
static std::shared_ptr<Ft5x06> ft5x06;
#else
static std::shared_ptr<Tt21100> tt21100;
#endif
static std::shared_ptr<espp::TouchpadInput> touchpad;

void i2c_write(uint8_t dev_addr, uint8_t *data, size_t len) {
  i2c_master_write_to_device(GAMEPAD_I2C_NUM,
                             dev_addr,
                             data,
                             len,
                             I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len) {
  i2c_master_write_read_device(GAMEPAD_I2C_NUM,
                               dev_addr,
                               &reg_addr,
                               1, // size of addr
                               read_data,
                               read_len,
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

#if USE_FT5X06
void ft5x06_write(uint8_t reg_addr, uint8_t value) {
  uint8_t write_buf[] = {reg_addr, value};
  i2c_master_write_to_device(TP_I2C_NUM,
                             Ft5x06::ADDRESS,
                             write_buf,
                             2,
                             I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void ft5x06_read(uint8_t reg_addr, uint8_t *read_data, size_t read_len) {
  i2c_master_write_read_device(TP_I2C_NUM,
                               Ft5x06::ADDRESS,
                               &reg_addr,
                               1, // size of addr
                               read_data,
                               read_len,
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}
#else
void tt21100_write(uint8_t reg_addr, uint8_t value) {
  uint8_t write_buf[] = {reg_addr, value};
  i2c_write(Tt21100::ADDRESS, write_buf, 2);
}

void tt21100_read(uint8_t *read_data, size_t read_len) {
  static const int I2C_BUS_MS_TO_WAIT = 1000;
  static const int I2C_BUS_TICKS_TO_WAIT = (I2C_BUS_MS_TO_WAIT/portTICK_PERIOD_MS);
  static uint8_t I2C_ACK_CHECK_EN = 1;
  static uint8_t cmd_buffer[I2C_LINK_RECOMMENDED_SIZE(4)];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(cmd_buffer, I2C_LINK_RECOMMENDED_SIZE(4));

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (Tt21100::ADDRESS << 1) | I2C_MASTER_READ, I2C_ACK_CHECK_EN);
  i2c_master_read(cmd, read_data, read_len, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);

  i2c_master_cmd_begin(TP_I2C_NUM, cmd, I2C_BUS_TICKS_TO_WAIT);
  i2c_cmd_link_delete_static(cmd);
}
#endif

#if USE_QWIICNES
void qwiicnes_write(uint8_t reg_addr, uint8_t value) {
  uint8_t write_buf[] = {reg_addr, value};
  i2c_write(QwiicNes::ADDRESS, write_buf, 2);
}

uint8_t qwiicnes_read(uint8_t reg_addr) {
  uint8_t data;
  i2c_read(QwiicNes::ADDRESS, reg_addr, &data, 1);
  return data;
}
#else
void ads_write(uint8_t reg_addr, uint16_t value) {
  uint8_t write_buf[3] = {reg_addr, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
  i2c_write(Ads1x15::ADDRESS, write_buf, 3);
}

uint16_t ads_read(uint8_t reg_addr) {
  uint8_t data[2];
  i2c_read(Ads1x15::ADDRESS, reg_addr, (uint8_t*)&data, 2);
  return (data[0] << 8) | data[1];
}

static std::atomic<float> joystick_x{0};
static std::atomic<float> joystick_y{0};

void ads_read_task_fn(std::mutex& m, std::condition_variable& cv) {
  // NOTE: sleeping in this way allows the sleep to exit early when the
  // task is being stopped / destroyed
  {
    using namespace std::chrono_literals;
    std::unique_lock<std::mutex> lk(m);
    cv.wait_for(lk, 20ms);
  }
  auto x_mv = ads->sample_mv(1);
  auto y_mv = ads->sample_mv(0);
  joystick_x.store(x_mv / 1700.0f - 1.0f);
  // y is inverted so negate it
  joystick_y.store(-(y_mv / 1700.0f - 1.0f));
};

extern "C" bool read_joystick(float *x, float *y) {
  *x = joystick_x.load();
  *y = joystick_y.load();
  return true;
}
#endif // else !USE_QWIICNES

std::atomic<bool> user_quit_{false};
extern "C" void reset_user_quit() {
  user_quit_ = false;
}

extern "C" bool user_quit() {
  // TODO: allow select + start to trigger this condition as well?
  return user_quit_;
}

extern "C" void touchpad_read(uint8_t* num_touch_points, uint16_t* x, uint16_t* y, uint8_t* btn_state) {
  // NOTE: ft5x06 does not have button support, so data->btn_val cannot be set
#if USE_FT5X06
  ft5x06->get_touch_point(num_touch_points, x, y);
#else
  // get the latest data from the device
  while (!tt21100->read())
    ;
  // now hand it off
  tt21100->get_touch_point(num_touch_points, x, y);
  *btn_state = tt21100->get_home_button_state();
  user_quit_ = (bool)(*btn_state);
#endif
}

static std::atomic<bool> initialized = false;
extern "C" void init_input() {
  if (initialized) return;
  fmt::print("Initializing input drivers...\n");

  fmt::print("initializing i2c driver...\n");
  memset(&i2c_cfg, 0, sizeof(i2c_cfg));
  i2c_cfg.sda_io_num = I2C_SDA_IO;
  i2c_cfg.scl_io_num = I2C_SCL_IO;
  i2c_cfg.mode = I2C_MODE_MASTER;
  i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
  i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
  i2c_cfg.master.clk_speed = I2C_FREQ_HZ;
  auto err = i2c_param_config(GAMEPAD_I2C_NUM, &i2c_cfg);
  if (err != ESP_OK) printf("config i2c failed\n");
  err = i2c_driver_install(GAMEPAD_I2C_NUM, I2C_MODE_MASTER,  0, 0, 0);
  if (err != ESP_OK) printf("install i2c driver failed\n");

#if USE_FT5X06
  fmt::print("Initializing ft5x06\n");
  ft5x06 = std::make_shared<Ft5x06>(Ft5x06::Config{
      .write = ft5x06_write,
      .read = ft5x06_read,
      .log_level = espp::Logger::Verbosity::WARN
    });
#else
  fmt::print("Initializing Tt21100\n");
  tt21100 = std::make_shared<Tt21100>(Tt21100::Config{
      .read = tt21100_read,
      .log_level = espp::Logger::Verbosity::WARN
    });
#endif

  fmt::print("Initializing touchpad\n");
  touchpad = std::make_shared<espp::TouchpadInput>(espp::TouchpadInput::Config{
      .touchpad_read = touchpad_read,
      .swap_xy = false,
      .invert_x = true,
      .invert_y = false,
      .log_level = espp::Logger::Verbosity::WARN
    });

#if USE_QWIICNES
  fmt::print("Making QwiicNES\n");
  qwiicnes = std::make_shared<QwiicNes>(QwiicNes::Config{
      .write = qwiicnes_write,
      .read = qwiicnes_read,
    });
#else
  fmt::print("initializing ADS1015\n");
  ads = std::make_shared<Ads1x15>(Ads1x15::Ads1015Config{
      .write = ads_write,
      .read = ads_read,
    });

  fmt::print("Making controller\n");
  controller = std::make_shared<Controller>(Controller::AnalogJoystickConfig{
      // buttons short to ground, so they are active low. this will enable the
      // GPIO_PULLUP and invert the logic
      .active_low = true,
      .gpio_a = 38,
      .gpio_b = 39,
      .gpio_x = -1,
      .gpio_y = -1,
      .gpio_start = 42,
      .gpio_select = 21,
      .gpio_joystick_select = -1,
      .joystick_config = {
        .x_calibration = {.center = 0.0f, .deadband = 0.2f, .minimum = -1.0f, .maximum = 1.0f},
        .y_calibration = {.center = 0.0f, .deadband = 0.2f, .minimum = -1.0f, .maximum = 1.0f},
        .get_values = read_joystick,
        .log_level = espp::Logger::Verbosity::WARN
      },
      .log_level = espp::Logger::Verbosity::WARN
    });

  fmt::print("making ads task\n");
  ads_task = espp::Task::make_unique({
      .name = "ADS",
      .callback = ads_read_task_fn,
      .stack_size_bytes{4*1024},
      .log_level = espp::Logger::Verbosity::INFO
    });
  ads_task->start();
#endif

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
#if USE_QWIICNES
  if (!qwiicnes) {
    fmt::print("cannot get input state: qwiicnes not initialized properly!\n");
    return;
  }
  qwiicnes->update();
  is_a_pressed = qwiicnes->is_pressed(QwiicNes::Button::A);
  is_b_pressed = qwiicnes->is_pressed(QwiicNes::Button::B);
  is_select_pressed = qwiicnes->is_pressed(QwiicNes::Button::SELECT);
  is_start_pressed = qwiicnes->is_pressed(QwiicNes::Button::START);
  is_up_pressed = qwiicnes->is_pressed(QwiicNes::Button::UP);
  is_down_pressed = qwiicnes->is_pressed(QwiicNes::Button::DOWN);
  is_left_pressed = qwiicnes->is_pressed(QwiicNes::Button::LEFT);
  is_right_pressed = qwiicnes->is_pressed(QwiicNes::Button::RIGHT);
#else
  if (!controller) {
    fmt::print("cannot get input state: controller not initialized properly!\n");
    return;
  }
  controller->update();
  auto s = controller->get_state();
  is_a_pressed = s.a;
  is_b_pressed = s.b;
  is_x_pressed = s.x;
  is_y_pressed = s.y;
  is_start_pressed = s.start;
  is_select_pressed = s.select;
  is_up_pressed = s.up;
  is_down_pressed = s.down;
  is_left_pressed = s.left;
  is_right_pressed = s.right;
#endif
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
