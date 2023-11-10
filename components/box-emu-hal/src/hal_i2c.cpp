#include "hal_i2c.hpp"

std::shared_ptr<espp::I2c> internal_i2c = nullptr;
std::shared_ptr<espp::I2c> external_i2c = nullptr;

static bool initialized = false;

using namespace box_hal;

void i2c_init() {
  if (initialized) return;
  internal_i2c = std::make_shared<espp::I2c>(espp::I2c::Config{
      .port = internal_i2c_port,
      .sda_io_num = internal_i2c_sda,
      .scl_io_num = internal_i2c_scl,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE});
  external_i2c = std::make_shared<espp::I2c>(espp::I2c::Config{
      .port = external_i2c_port,
      .sda_io_num = external_i2c_sda,
      .scl_io_num = external_i2c_scl,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE});
  initialized = true;
}
