#include "box_emu_hal.hpp"

static std::shared_ptr<espp::I2c> internal_i2c = nullptr;
static std::shared_ptr<espp::I2c> external_i2c = nullptr;

static bool initialized = false;

void hal::i2c_init() {
  if (initialized) return;
  // make the i2c on core 1 so that the i2c interrupts are handled on core 1
  espp::Task::run_on_core([]() {
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
  }, 1);
  initialized = true;
}

std::shared_ptr<espp::I2c> hal::get_internal_i2c() {
  i2c_init();
  return internal_i2c;
}

std::shared_ptr<espp::I2c> hal::get_external_i2c() {
  i2c_init();
  return external_i2c;
}
