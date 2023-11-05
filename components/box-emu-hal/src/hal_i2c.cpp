#include "hal_i2c.hpp"

// Only used for the touchpad and the imu
static constexpr i2c_port_t I2C_INTERNAL = I2C_NUM_0;
// used for our peripherals (external to the ESP S3 BOX)
static constexpr i2c_port_t I2C_EXTERNAL = I2C_NUM_1;
static constexpr int I2C_FREQ_HZ = (400*1000);
static constexpr int I2C_TIMEOUT_MS = 10;

std::shared_ptr<espp::I2c> internal_i2c = nullptr;
std::shared_ptr<espp::I2c> external_i2c = nullptr;

static bool initialized = false;

void i2c_init() {
  if (initialized) return;
  internal_i2c = std::make_shared<espp::I2c>(espp::I2c::Config{
      .port = I2C_INTERNAL,
      .sda_io_num = GPIO_NUM_8,
      .scl_io_num = GPIO_NUM_18,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE});
  external_i2c = std::make_shared<espp::I2c>(espp::I2c::Config{
      .port = I2C_EXTERNAL,
      .sda_io_num = GPIO_NUM_41,
      .scl_io_num = GPIO_NUM_40,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE});
  initialized = true;
}
