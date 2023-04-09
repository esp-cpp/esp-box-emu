#pragma once

#include "driver/i2c.h"

// Only used for the touchpad and the imu
static constexpr i2c_port_t I2C_INTERNAL = I2C_NUM_0;
// used for our peripherals (external to the ESP S3 BOX)
static constexpr i2c_port_t I2C_EXTERNAL = I2C_NUM_1;

void i2c_init();

void i2c_write_internal_bus(uint8_t dev_addr, uint8_t *data, size_t len);
void i2c_read_internal_bus(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len);
void i2c_read_internal_bus(uint8_t dev_addr, uint8_t* read_data, size_t read_len);

void i2c_write_external_bus(uint8_t dev_addr, uint8_t *data, size_t len);
void i2c_read_external_bus(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len);
