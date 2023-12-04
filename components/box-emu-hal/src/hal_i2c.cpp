#include "hal_i2c.hpp"

std::shared_ptr<espp::I2c> internal_i2c = nullptr;
std::shared_ptr<espp::I2c> external_i2c = nullptr;

static bool initialized = false;

using namespace box_hal;

void i2c_init() {
  if (initialized) return;
  // make the i2c on core 1 so that the i2c interrupts are handled on core 1
  auto i2c_task = espp::Task::make_unique(espp::Task::Config{
      .name = "i2c",
        .callback = [&](auto &m, auto&cv) -> bool {
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
        return true; // stop the task
      },
      .stack_size_bytes = 2*1024,
      .core_id = 1
    });
  i2c_task->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  initialized = true;
}
