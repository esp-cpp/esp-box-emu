#include "input.h"

#include "driver/i2c.h"

#include "controller.hpp"
#include "ads1x15.hpp"
#include "qwiicnes.hpp"
#include "task.hpp"

using namespace std::chrono_literals;

/* I2C port and GPIOs */
#define I2C_NUM         (I2C_NUM_1)
#define I2C_MASTER_NUM  (I2C_NUM_1)
#define I2C_SCL_IO      (GPIO_NUM_40)
#define I2C_SDA_IO      (GPIO_NUM_41)
#define I2C_FREQ_HZ     (400 * 1000)                     /*!< I2C master clock frequency */
#define I2C_TIMEOUT_MS         1000
#define I2C_MASTER_TIMEOUT_MS (10)

static std::shared_ptr<Controller> controller;
static std::shared_ptr<Ads1x15> ads;
static std::shared_ptr<QwiicNes> qwiicnes;
static std::unique_ptr<espp::Task> ads_task;

void i2c_write(uint8_t dev_addr, uint8_t *data, size_t len) {
  i2c_master_write_to_device(I2C_MASTER_NUM,
                             dev_addr,
                             data,
                             len,
                             I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len) {
  i2c_master_write_read_device(I2C_MASTER_NUM,
                               dev_addr,
                               &reg_addr,
                               1, // size of addr
                               read_data,
                               read_len,
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void qwiicnes_write(uint8_t reg_addr, uint8_t value) {
  uint8_t write_buf[] = {reg_addr, value};
  i2c_write(QwiicNes::ADDRESS, write_buf, 2);
}

uint8_t qwiicnes_read(uint8_t reg_addr) {
  uint8_t data;
  i2c_read(QwiicNes::ADDRESS, reg_addr, &data, 1);
  return data;
}

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
  // fmt::print("joystick x,y: ({}, {}) -> ({}, {})\n", x_raw, y_raw, x_mv, y_mv);
  joystick_x.store(x_mv / 1.7f - 1.0f);
  // y is inverted so negate it
  joystick_y.store(-(y_mv / 1.7f - 1.0f));
};

extern "C" bool read_joystick(float *x, float *y) {
  *x = joystick_x.load();
  *y = joystick_y.load();
  return true;
}

static std::atomic<bool> initialized = false;
extern "C" void init_input() {
  if (initialized) return;
  fmt::print("Initializing input drivers...\n");

  fmt::print("initializing i2c driver...\n");
  i2c_config_t es_i2c_cfg;
  memset(&es_i2c_cfg, 0, sizeof(es_i2c_cfg));
  es_i2c_cfg.sda_io_num = I2C_SDA_IO;
  es_i2c_cfg.scl_io_num = I2C_SCL_IO;
  es_i2c_cfg.mode = I2C_MODE_MASTER;
  es_i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
  es_i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
  es_i2c_cfg.master.clk_speed = I2C_FREQ_HZ;
  auto err = i2c_param_config(I2C_NUM, &es_i2c_cfg);
  if (err != ESP_OK) printf("config i2c failed\n");
  err = i2c_driver_install(I2C_NUM, I2C_MODE_MASTER,  0, 0, 0);
  if (err != ESP_OK) printf("install i2c driver failed\n");

  fmt::print("initializing ADS1015\n");
  ads = std::make_shared<Ads1x15>(Ads1x15::Ads1015Config{
      .write = ads_write,
      .read = ads_read,
    });

  fmt::print("Making QwiicNES\n");
  qwiicnes = std::make_shared<QwiicNes>(QwiicNes::Config{
      .write = qwiicnes_write,
      .read = qwiicnes_read,
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

  initialized = true;
}

extern "C" void get_input_state(struct InputState* state) {
  if (!controller) {
    fmt::print("cannot get input state: controller not initialized properly!\n");
    return;
  }
  controller->update();
  auto s = controller->get_state();
  state->a = s.a;
  state->b = s.b;
  state->x = s.x;
  state->y = s.y;
  state->start = s.start;
  state->select = s.select;
  state->up = s.up;
  state->down = s.down;
  state->left = s.left;
  state->right = s.right;
}
