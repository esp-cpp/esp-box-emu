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

void i2c_write_internal_bus(uint8_t dev_addr, uint8_t *data, size_t len) {
  i2c_master_write_to_device(I2C_INTERNAL,
                             dev_addr,
                             data,
                             len,
                             I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void i2c_read_internal_bus(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len) {
  i2c_master_write_read_device(I2C_INTERNAL,
                               dev_addr,
                               &reg_addr,
                               1, // size of addr
                               read_data,
                               read_len,
                               I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void i2c_read_internal_bus(uint8_t dev_addr, uint8_t* read_data, size_t read_len) {
  static const int I2C_BUS_MS_TO_WAIT = 1000;
  static const int I2C_BUS_TICKS_TO_WAIT = (I2C_BUS_MS_TO_WAIT/portTICK_PERIOD_MS);
  static uint8_t I2C_ACK_CHECK_EN = 1;
  static uint8_t cmd_buffer[I2C_LINK_RECOMMENDED_SIZE(4)];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(cmd_buffer, I2C_LINK_RECOMMENDED_SIZE(4));

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, I2C_ACK_CHECK_EN);
  i2c_master_read(cmd, read_data, read_len, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);

  i2c_master_cmd_begin(I2C_INTERNAL, cmd, I2C_BUS_TICKS_TO_WAIT);
  i2c_cmd_link_delete_static(cmd);
}

void i2c_write_external_bus(uint8_t dev_addr, uint8_t *data, size_t len) {
  i2c_master_write_to_device(I2C_EXTERNAL,
                             dev_addr,
                             data,
                             len,
                             I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void i2c_read_external_bus(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len) {
  i2c_master_write_read_device(I2C_EXTERNAL,
                               dev_addr,
                               &reg_addr,
                               1, // size of addr
                               read_data,
                               read_len,
                               I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void i2c_read_external_bus(uint8_t dev_addr, uint8_t* read_data, size_t read_len) {
  static const int I2C_BUS_MS_TO_WAIT = 1000;
  static const int I2C_BUS_TICKS_TO_WAIT = (I2C_BUS_MS_TO_WAIT/portTICK_PERIOD_MS);
  static uint8_t I2C_ACK_CHECK_EN = 1;
  static uint8_t cmd_buffer[I2C_LINK_RECOMMENDED_SIZE(4)];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(cmd_buffer, I2C_LINK_RECOMMENDED_SIZE(4));

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, I2C_ACK_CHECK_EN);
  i2c_master_read(cmd, read_data, read_len, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);

  i2c_master_cmd_begin(I2C_EXTERNAL, cmd, I2C_BUS_TICKS_TO_WAIT);
  i2c_cmd_link_delete_static(cmd);
}
