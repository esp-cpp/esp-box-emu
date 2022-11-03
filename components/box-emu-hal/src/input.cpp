#include "input.h"

#include "driver/i2c.h"

#include "controller.hpp"
#include "ads1x15.h"
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
static std::unique_ptr<espp::Task> ads_task;

// static std::shared_ptr<idf::I2CMaster> i2c;
static constexpr int16_t gain_ = GAIN_TWOTHIRDS; // GAIN_ONE;  // +/- 4.096V range
static constexpr int bit_shift_ = 4;

#define ADS1015_ADDRESS (0x48)

void ads_write(uint8_t reg_addr, uint16_t value) {
  uint8_t write_buf[3] = {reg_addr, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
  i2c_master_write_to_device(I2C_MASTER_NUM,
                             ADS1015_ADDRESS,
                             write_buf,
                             sizeof(write_buf),
                             I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

uint16_t ads_read(uint8_t reg_addr) {
  uint8_t data[2];
  i2c_master_write_read_device(I2C_MASTER_NUM,
                               ADS1015_ADDRESS,
                               &reg_addr,
                               1, // size of addr
                               (uint8_t*)&data,
                               2, // amount of data to read
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  return (data[0] << 8) | data[1];
}

bool conversion_complete() {
  return (ads_read(ADS1X15_REG_POINTER_CONFIG) & 0x8000) != 0;
}

int16_t ads_sample(int channel) {
  // Start with default values
  uint16_t config =
      ADS1X15_REG_CONFIG_CQUE_1CONV |   // Comparator enabled and asserts on 1
                                        // match
      ADS1X15_REG_CONFIG_CLAT_NONLAT |   // non-latching (default val)
      ADS1X15_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
      ADS1X15_REG_CONFIG_CMODE_TRAD |   // Traditional comparator (default val)
      ADS1X15_REG_CONFIG_MODE_SINGLE;   // Single conversion mode
  // Set PGA/voltage range
  config |= gain_;
  // Set data rate
  config |= RATE_ADS1015_1600SPS;
  config |= MUX_BY_CHANNEL[channel];
  // Set 'start single-conversion' bit
  config |= ADS1X15_REG_CONFIG_OS_SINGLE;
  // configure to read from mux 0
  // fmt::print("configuring conversion for channel {}\n", channel);
  ads_write(ADS1X15_REG_POINTER_CONFIG, config);
  ads_write(ADS1X15_REG_POINTER_HITHRESH, 0x8000);
  ads_write(ADS1X15_REG_POINTER_LOWTHRESH, 0x0000);
  // wait for conversion complete
  //fmt::print("waiting for conversion complete...\n");
  while (!conversion_complete()) {
    vTaskDelay(10);
  }
  // fmt::print("reading conversion result for channel {}\n", channel);
  uint16_t val = ads_read(ADS1X15_REG_POINTER_CONVERT) >> bit_shift_;
  if (bit_shift_ > 0) {
    if (val > 0x07FF) {
      // negative number - extend the sign to the 16th bit
      val |= 0xF000;
    }
  }
  return (int16_t)val;
}

float ads_raw_to_mv(int16_t raw) {
  // see data sheet Table 3
  float fsRange;
  switch (gain_) {
  case GAIN_TWOTHIRDS:
    fsRange = 6.144f;
    break;
  case GAIN_ONE:
    fsRange = 4.096f;
    break;
  case GAIN_TWO:
    fsRange = 2.048f;
    break;
  case GAIN_FOUR:
    fsRange = 1.024f;
    break;
  case GAIN_EIGHT:
    fsRange = 0.512f;
    break;
  case GAIN_SIXTEEN:
    fsRange = 0.256f;
    break;
  default:
    fsRange = 0.0f;
  }
  return raw * (fsRange / (32768 >> bit_shift_));
}

static std::atomic<float> joystick_x{0};
static std::atomic<float> joystick_y{0};

void ads_read_task_fn(std::mutex& m, std::condition_variable& cv) {
  // NOTE: sleeping in this way allows the sleep to exit early when the
  // task is being stopped / destroyed
  {
    using namespace std::chrono_literals;
    std::unique_lock<std::mutex> lk(m);
    cv.wait_for(lk, 50ms);
  }
  // fmt::print("sampling ADS channel 1\n");
  auto x_raw = ads_sample(1);
  // fmt::print("sampling ADS channel 0\n");
  auto y_raw = ads_sample(0);
  auto x_mv = ads_raw_to_mv(x_raw); // should be range [0, 3.3]
  auto y_mv = ads_raw_to_mv(y_raw); // should be range [0, 3.3]
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
