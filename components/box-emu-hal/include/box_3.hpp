#pragma once

#include "hal/spi_types.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/spi_master.h"

#include "i2c.hpp"
#include "st7789.hpp"
#include "touchpad_input.hpp"
#include "gt911.hpp"

static constexpr std::string_view dev_kit = "ESP32-S3-BOX-3";

// internal i2c (touchscreen, audio codec)
static constexpr auto internal_i2c_port = I2C_NUM_0;
static constexpr auto internal_i2c_clock_speed = 400 * 1000;
static constexpr gpio_num_t internal_i2c_sda = GPIO_NUM_8;
static constexpr gpio_num_t internal_i2c_scl = GPIO_NUM_18;

// external I2c (peripherals)
static constexpr auto external_i2c_port = I2C_NUM_1;
static constexpr auto external_i2c_clock_speed = 400 * 1000;
static constexpr gpio_num_t external_i2c_sda = GPIO_NUM_41;
static constexpr gpio_num_t external_i2c_scl = GPIO_NUM_40;

// LCD
static constexpr int lcd_clock_speed = 60 * 1000 * 1000;
static constexpr auto lcd_spi_num = SPI2_HOST;
static constexpr gpio_num_t lcd_cs_io = GPIO_NUM_5;
static constexpr gpio_num_t lcd_mosi_io = GPIO_NUM_6;
static constexpr gpio_num_t lcd_sclk_io = GPIO_NUM_7;
static constexpr gpio_num_t lcd_reset_io = GPIO_NUM_48;
static constexpr gpio_num_t lcd_dc_io = GPIO_NUM_4;
static constexpr gpio_num_t backlight_io = GPIO_NUM_47; // was 45 on ESP32-S3-BOX
static constexpr size_t lcd_width = 320;
static constexpr size_t lcd_height = 240;
static constexpr bool backlight_value = true;
static constexpr bool reset_value = true; // was false on ESP32-S3-BOX
static constexpr bool invert_colors = true;
static constexpr auto rotation = espp::Display::Rotation::LANDSCAPE;
static constexpr bool mirror_x = true;
static constexpr bool mirror_y = true;
using DisplayDriver = espp::St7789;

// touch
static constexpr bool touch_swap_xy = false;
static constexpr bool touch_invert_x = false;
static constexpr bool touch_invert_y = false;
static constexpr gpio_num_t touch_interrupt = GPIO_NUM_3;
using TouchDriver = espp::Gt911;

// sound
static constexpr gpio_num_t sound_power_pin = GPIO_NUM_46;
static constexpr auto  i2s_port = I2S_NUM_0;
static constexpr gpio_num_t i2s_mck_io = GPIO_NUM_2;
static constexpr gpio_num_t i2s_bck_io = GPIO_NUM_17;
static constexpr gpio_num_t i2s_ws_io = GPIO_NUM_45; // was 47 on ESP32-S3-BOX
static constexpr gpio_num_t i2s_do_io = GPIO_NUM_15;
static constexpr gpio_num_t i2s_di_io = GPIO_NUM_16;
static constexpr gpio_num_t mute_pin = GPIO_NUM_1;

// uSD card
static constexpr gpio_num_t sdcard_cs = GPIO_NUM_10;
static constexpr gpio_num_t sdcard_mosi = GPIO_NUM_11;
static constexpr gpio_num_t sdcard_miso = GPIO_NUM_13;
static constexpr gpio_num_t sdcard_sclk = GPIO_NUM_12;
static constexpr auto sdcard_spi_num = SPI3_HOST;
