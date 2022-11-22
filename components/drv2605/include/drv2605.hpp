#pragma once

#include <functional>

#include "logger.hpp"

class Drv2605 {
public:
  static constexpr uint8_t ADDRESS = (0x5A);

  /**
   * @brief Function to write a byte to a register
   * @param uint8_t register address to write to
   * @param uint8_t data to write
   */
  typedef std::function<void(uint8_t, uint8_t)> write_fn;

  /**
   * @brief Function to read a byte from a register
   * @param uint8_t register address to read from
   * @return Byte read from the register
   */
  typedef std::function<uint8_t(uint8_t)> read_fn;

  enum class Mode : uint8_t {
    INTTRIG,     ///< Internal Trigger (call star() to start playback)
    EXTTRIGEDGE, ///< External edge trigger (rising edge on IN pin starts playback)
    EXTTRIGLVL,  ///< External level trigger (playback follows state of IN pin)
    PWMANALOG,   ///< PWM/Analog input
    AUDIOVIBE,   ///< Audio-to-vibe mode
    REALTIME,    ///< Real-time playback (RTP)
    DIAGNOS,     ///< Diagnostics
    AUTOCAL,     ///< Auto-calibration
  };

  // See https://learn.adafruit.com/assets/72593 for the complete list
  enum class Waveform : uint8_t {
    END = 0, ///< Signals this is the end of the waveforms to play
    STRONG_CLICK = 1,
    SHARP_CLICK = 4,
    SOFT_BUMP = 7,
    DOUBLE_CLICK = 10,
    TRIPLE_CLICK = 12,
    SOFT_FUZZ = 13,
    STRONG_BUZZ = 13,
    ALERT_750MS = 15,
    ALERT_1000MS = 16, // omg there are 123 of theese i'm not typing them out right now...
    BUZZ1 = 47,
    BUZZ2 = 48,
    BUZZ3 = 49,
    BUZZ4 = 50,
    BUZZ5 = 51,
    PULSING_STRONG_1 = 52,
    PULSING_STRONG_2 = 53,
    TRANSITION_CLICK_1 = 58,
    TRANSITION_HUM_1 = 64,
  };

  /**
   *  @brief The type of vibration motor connected to the Drv2605
   */
  enum class MotorType {
    ERM, ///< Eccentric Rotating Mass (more common, therefore default)
    LRA  ///< Linear Resonant Actuator
  };

  struct Config {
    write_fn write;
    read_fn read;
    MotorType motor_type{MotorType::ERM};
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  Drv2605(const Config& config)
    : write_(config.write), read_(config.read),
      logger_({.tag="Drv2605", .level = config.log_level}){
    init(config);
  }

  void start() {
    logger_.info("Starting");
    write_((uint8_t)Register::START, 1);
  }

  void stop() {
    logger_.info("Stopping");
    write_((uint8_t)Register::START, 0);
  }

  void set_mode(Mode mode) {
    logger_.info("Setting mode {}", (uint8_t)mode);
    write_((uint8_t)Register::MODE, (uint8_t)mode);
  }

  void set_waveform(uint8_t slot, Waveform w) {
    logger_.info("Setting waveform {}", (uint8_t)w);
    write_((uint8_t)Register::WAVESEQ1 + slot, (uint8_t)w);
  }

  /**
   * @brief Select the waveform library to use.
   * @param lib Library to use, 0=Empty, 1-5 are ERM, 6 is LRA
   */
  void select_library(uint8_t lib) {
    logger_.info("Selecting library {}", lib);
    write_((uint8_t)Register::LIBRARY, lib);
  }

protected:
  void init (const Config& config) {
    logger_.info("Initializing motor");
    write_((uint8_t)Register::MODE, 0);  // out of standby
    write_((uint8_t)Register::RTPIN, 0); // no real-time playback
    set_waveform(0, Waveform::STRONG_CLICK);  // Strong Click
    set_waveform(1, Waveform::END);           // end sequence
    write_((uint8_t)Register::OVERDRIVE, 0);  // no overdrive
    write_((uint8_t)Register::SUSTAINPOS, 0);
    write_((uint8_t)Register::SUSTAINNEG, 0);
    write_((uint8_t)Register::BREAK, 0);
    write_((uint8_t)Register::AUDIOMAX, 0x64);
    // set the motor type based on the config
    set_motor_type(config.motor_type);
    // turn on ERM OPEN LOOP
    auto current_control3 = read_((uint8_t)Register::CONTROL3);
    write_((uint8_t)Register::CONTROL3, current_control3 | 0x20);
  }

  void set_motor_type(MotorType motor_type) {
    logger_.info("Setting motor type {}", motor_type == MotorType::ERM ? "ERM" : "LRA");
    auto current_feedback = read_((uint8_t)Register::FEEDBACK);
    uint8_t motor_config = (motor_type == MotorType::ERM) ? 0x7F : 0x80;
    write_((uint8_t)Register::FEEDBACK, current_feedback | motor_config);
  }

  enum class Register : uint8_t {
    STATUS = 0x00,   ///< Status
    MODE = 0x01,     ///< Mode
    RTPIN = 0x02,    ///< Real-Time playback input
    LIBRARY = 0x03,  ///< Waveform library selection
    WAVESEQ1 = 0x04, ///< Waveform sequence 1
    WAVESEQ2 = 0x05, ///< Waveform sequence 2
    WAVESEQ3 = 0x06, ///< Waveform sequence 3
    WAVESEQ4 = 0x07, ///< Waveform sequence 4
    WAVESEQ5 = 0x08, ///< Waveform sequence 5
    WAVESEQ6 = 0x09, ///< Waveform sequence 6
    WAVESEQ7 = 0x0A, ///< Waveform sequence 7
    WAVESEQ8 = 0x0B, ///< Waveform sequence 8
    START = 0x0C,    ///< Start/Stop playback control
    OVERDRIVE = 0x0D,///< Overdrive time offset
    SUSTAINPOS= 0x0E,///< Sustain time offset (positive)
    SUSTAINNEG= 0x0F,///< Sustain time offset (negative)
    BREAK = 0x10,    ///< Break time offset
    AUDIOCTRL = 0x11,///< Audio to vibe control
    AUDIOMIN = 0x12, ///< Audio to vibe min input level
    AUDIOMAX = 0x12, ///< Audio to vibe max input level
    AUDIOOUTMIN=0x14,///< Audio to vibe min output drive
    AUDIOOUTMAX=0x15,///< Audio to vibe max output drive
    RATEDV = 0x16,   ///< Rated voltage
    CLAMPV = 0x17,   ///< Overdrive clamp
    AUTOCALCOMP=0x18,///< Auto calibration compensation result
    AUTOCALEMP =0x19,///< Auto calibration back-EMF result
    FEEDBACK = 0x1A, ///< Feedback control
    CONTROL1 = 0x1B, ///< Control1
    CONTROL2 = 0x1C, ///< Control2
    CONTROL3 = 0x1D, ///< Control3
    CONTROL4 = 0x1E, ///< Control4
    VBAT = 0x21,     ///< Vbat voltage monitor
    LRARSON = 0x22,  ///< LRA resonance-period
  };

  write_fn write_;
  read_fn read_;
  espp::Logger logger_;
};
