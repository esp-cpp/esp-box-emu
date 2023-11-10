#pragma once

#include <memory>

#include "hal.hpp"
#include "i2c.hpp"

extern std::shared_ptr<espp::I2c> internal_i2c;
extern std::shared_ptr<espp::I2c> external_i2c;

void i2c_init();
