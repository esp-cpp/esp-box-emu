#pragma once

#include <functional>

#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/task.h"

#include "logger.hpp"

class Ads1x15 {
public:
  static constexpr uint8_t ADDRESS = (0x48); ///< 1001 000 (ADDR = GND)

  typedef std::function<void(uint8_t, uint16_t)> write_fn;
  typedef std::function<uint16_t(uint8_t)> read_fn;

  enum class Gain {
    TWOTHIRDS = 0x0000, ///< +/-6.144V range = Gain 2/3
    ONE =       0x0200, ///< +/-4.096V range = Gain 1
    TWO =       0x0400, ///< +/-2.048V range = Gain 2 (default)
    FOUR =      0x0600, ///< +/-1.024V range = Gain 4
    EIGHT =     0x0800, ///< +/-0.512V range = Gain 8
    SIXTEEN =   0x0A00, ///< +/-0.256V range = Gain 16
  };

  enum class Ads1015Rate : uint16_t {
    SPS128  = 0x0000, ///< 128 samples per second
    SPS250  = 0x0020, ///< 250 samples per second
    SPS490  = 0x0040, ///< 490 samples per second
    SPS920  = 0x0060, ///< 920 samples per second
    SPS1600 = 0x0080, ///< 1600 samples per second (default)
    SPS2400 = 0x00A0, ///< 2400 samples per second
    SPS3300 = 0x00C0, ///< 3300 samples per second
  };

  enum class Ads1115Rate : uint16_t {
    SPS8   = 0x0000, ///< 8 samples per second
    SPS16  = 0x0020, ///< 16 samples per second
    SPS32  = 0x0040, ///< 32 samples per second
    SPS64  = 0x0060, ///< 64 samples per second
    SPS128 = 0x0080, ///< 128 samples per second (default)
    SPS250 = 0x00A0, ///< 250 samples per second
    SPS475 = 0x00C0, ///< 475 samples per second
    SPS860 = 0x00E0, ///< 860 samples per second
  };

  struct Ads1015Config {
    write_fn write;
    read_fn read;
    Gain gain{Gain::TWOTHIRDS};
    Ads1015Rate sample_rate{Ads1015Rate::SPS1600};
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  struct Ads1115Config {
    write_fn write;
    read_fn read;
    Gain gain{Gain::TWOTHIRDS};
    Ads1115Rate sample_rate{Ads1115Rate::SPS128};
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  Ads1x15(const Ads1015Config& config)
    : gain_(config.gain),
      ads1015rate_(config.sample_rate),
      bit_shift_(4), write_(config.write), read_(config.read),
      logger_({.tag="Ads1015", .level = config.log_level}){
  }

  Ads1x15(const Ads1115Config& config)
    : gain_(config.gain),
      ads1115rate_(config.sample_rate),
      bit_shift_(0), write_(config.write), read_(config.read),
      logger_({.tag="Ads1115", .level = config.log_level}){
  }

  float sample_mv(int channel) {
    return raw_to_mv(sample_raw(channel));
  }

protected:
  int16_t sample_raw(int channel);

  bool conversion_complete();

  float raw_to_mv(int16_t raw) {
    // see data sheet Table 3
    float fsRange;
    switch (gain_) {
    case Gain::TWOTHIRDS:
      fsRange = 6.144f;
      break;
    case Gain::ONE:
      fsRange = 4.096f;
      break;
    case Gain::TWO:
      fsRange = 2.048f;
      break;
    case Gain::FOUR:
      fsRange = 1.024f;
      break;
    case Gain::EIGHT:
      fsRange = 0.512f;
      break;
    case Gain::SIXTEEN:
      fsRange = 0.256f;
      break;
    default:
      fsRange = 0.0f;
    }
    return raw * (fsRange / (32768 >> bit_shift_));
  }

  enum class Register : uint8_t {
    POINTER_CONVERT = 0x00,   ///< Conversion
    POINTER_CONFIG = 0x01,    ///< Configuration
    POINTER_LOWTHRESH = 0x02, ///< Low Threshold
    POINTER_HITHRESH = 0x03   ///< High Threshold
  };

  Gain gain_;
  union {
    Ads1015Rate ads1015rate_;
    Ads1115Rate ads1115rate_;
    uint16_t rate_;
  };
  int bit_shift_;
  write_fn write_;
  read_fn read_;
  espp::Logger logger_;
};
