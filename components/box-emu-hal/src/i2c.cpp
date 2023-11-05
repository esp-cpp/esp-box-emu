#include "i2c.hpp"

#include <atomic>
#include <cstring>

#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"

#include "format.hpp"

static constexpr int I2C_FREQ_HZ = (400*1000);
static constexpr int I2C_TIMEOUT_MS = 10;

static std::atomic<bool> initialized = false;
void i2c_init() {
  if (initialized) return;

  esp_err_t err;

  fmt::print("initializing internal i2c driver...\n");
  i2c_config_t internal_i2c_cfg;
  memset(&internal_i2c_cfg, 0, sizeof(internal_i2c_cfg));
  internal_i2c_cfg.sda_io_num = GPIO_NUM_8;
  internal_i2c_cfg.scl_io_num = GPIO_NUM_18;
  internal_i2c_cfg.mode = I2C_MODE_MASTER;
  internal_i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
  internal_i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
  internal_i2c_cfg.master.clk_speed = I2C_FREQ_HZ;
  err = i2c_param_config(I2C_INTERNAL, &internal_i2c_cfg);
  if (err != ESP_OK) printf("config i2c failed\n");
  err = i2c_driver_install(I2C_INTERNAL, I2C_MODE_MASTER,  0, 0, 0); // buff len (x2), default flags
  if (err != ESP_OK) printf("install i2c driver failed\n");

  fmt::print("initializing external i2c driver...\n");
  i2c_config_t external_i2c_cfg;
  memset(&external_i2c_cfg, 0, sizeof(external_i2c_cfg));
  external_i2c_cfg.sda_io_num = GPIO_NUM_41;
  external_i2c_cfg.scl_io_num = GPIO_NUM_40;
  external_i2c_cfg.mode = I2C_MODE_MASTER;
  external_i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
  external_i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
  external_i2c_cfg.master.clk_speed = I2C_FREQ_HZ;
  err = i2c_param_config(I2C_EXTERNAL, &external_i2c_cfg);
  if (err != ESP_OK) printf("config i2c failed\n");
  err = i2c_driver_install(I2C_EXTERNAL, I2C_MODE_MASTER,  0, 0, 0);
  if (err != ESP_OK) printf("install i2c driver failed\n");

  initialized = true;
}

void i2c_deinit() {
  i2c_driver_delete(I2C_INTERNAL);
  i2c_driver_delete(I2C_EXTERNAL);
}

bool i2c_write_internal_bus(uint8_t dev_addr, uint8_t *data, size_t len) {
  auto err = i2c_master_write_to_device(I2C_INTERNAL,
                             dev_addr,
                             data,
                             len,
                             I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
  return err == ESP_OK;
}

bool i2c_read_internal_bus(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len) {
  auto err = i2c_master_write_read_device(I2C_INTERNAL,
                               dev_addr,
                               &reg_addr,
                               1, // size of addr
                               read_data,
                               read_len,
                               I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
  return err == ESP_OK;
}

bool i2c_read_internal_bus(uint8_t dev_addr, uint8_t* read_data, size_t read_len) {
  auto err = i2c_master_read_from_device(I2C_INTERNAL,
                               dev_addr,
                               read_data,
                               read_len,
                               I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
  return err == ESP_OK;
}

bool i2c_write_external_bus(uint8_t dev_addr, uint8_t *data, size_t len) {
  auto err = i2c_master_write_to_device(I2C_EXTERNAL,
                             dev_addr,
                             data,
                             len,
                             I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
  return err == ESP_OK;
}

bool i2c_read_external_bus(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len) {
  auto err = i2c_master_write_read_device(I2C_EXTERNAL,
                               dev_addr,
                               &reg_addr,
                               1, // size of addr
                               read_data,
                               read_len,
                               I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
  return err == ESP_OK;
}
