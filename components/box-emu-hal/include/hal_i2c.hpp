#pragma once

#include <memory>

#include "hal.hpp"
#include "i2c.hpp"

namespace hal {
  void i2c_init();
  std::shared_ptr<espp::I2c> get_internal_i2c();
  std::shared_ptr<espp::I2c> get_external_i2c();
} // namespace hal
