#pragma once

#include <cstdint>

#include "aw9523.hpp"
#include "oneshot_adc.hpp"

class EmuV1 {
public:
  using InputDriver = espp::Aw9523;
  static constexpr gpio_num_t VBAT_SENSE_PIN = GPIO_NUM_14; // battery sense pin is on GPIO 14
  static constexpr gpio_num_t AW9523_INT_PIN = GPIO_NUM_21; // interrupt pin is on GPIO 21
  static constexpr uint16_t UP_PIN =    (1<<0) << 0; // up pin is on port 0 of the AW9523
  static constexpr uint16_t DOWN_PIN =  (1<<1) << 0; // down pin is on port 0 of the AW9523
  static constexpr uint16_t LEFT_PIN =  (1<<2) << 0; // left pin is on port 0 of the AW9523
  static constexpr uint16_t RIGHT_PIN = (1<<3) << 0; // right pin is on port 0 of the AW9523
  static constexpr uint16_t A_PIN =     (1<<4) << 0; // a pin is on port 0 of the AW9523
  static constexpr uint16_t B_PIN =     (1<<5) << 0; // b pin is on port 0 of the AW9523
  static constexpr uint16_t X_PIN =     (1<<6) << 0; // x pin is on port 0 of the AW9523
  static constexpr uint16_t Y_PIN =     (1<<7) << 0; // y pin is on port 0 of the AW9523
  static constexpr uint16_t START_PIN =     (1<<0) << 8; // start pin is on port 1 of the AW9523
  static constexpr uint16_t SELECT_PIN =    (1<<1) << 8; // select pin is on port 1 of the AW9523
  static constexpr uint16_t BAT_ALERT_PIN = (1<<3) << 8; // battery alert pin is on port 1 of the AW9523
  static constexpr uint16_t VOL_UP_PIN =    (1<<4) << 8; // volume up pin is on port 1 of the AW9523
  static constexpr uint16_t VOL_DOWN_PIN =  (1<<5) << 8; // volume down pin is on port 1 of the AW9523
  static constexpr uint16_t DIRECTION_MASK = (UP_PIN | DOWN_PIN | LEFT_PIN | RIGHT_PIN | A_PIN | B_PIN | X_PIN | Y_PIN | START_PIN | SELECT_PIN | BAT_ALERT_PIN | VOL_UP_PIN | VOL_DOWN_PIN);
  static constexpr uint16_t INTERRUPT_MASK = (BAT_ALERT_PIN);
  static constexpr uint16_t INVERT_MASK = (UP_PIN | DOWN_PIN | LEFT_PIN | RIGHT_PIN | A_PIN | B_PIN | X_PIN | Y_PIN | START_PIN | SELECT_PIN | BAT_ALERT_PIN | VOL_UP_PIN | VOL_DOWN_PIN); // pins are active low so invert them
  static constexpr uint8_t PORT_0_DIRECTION_MASK = DIRECTION_MASK & 0xFF;
  static constexpr uint8_t PORT_1_DIRECTION_MASK = (DIRECTION_MASK >> 8) & 0xFF;
  static constexpr uint8_t PORT_0_INTERRUPT_MASK = INTERRUPT_MASK & 0xFF;
  static constexpr uint8_t PORT_1_INTERRUPT_MASK = (INTERRUPT_MASK >> 8) & 0xFF;

  // ADC for the battery voltage, it's on ADC2_CH3, which is IO14
  static constexpr adc_unit_t BATTERY_ADC_UNIT = ADC_UNIT_2;
  static constexpr adc_channel_t BATTERY_ADC_CHANNEL = ADC_CHANNEL_3;
};
