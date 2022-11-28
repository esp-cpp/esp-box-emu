#pragma once

#include <atomic>
#include <functional>
#include <cmath>

#include "logger.hpp"
#include "task.hpp"

/**
 * @brief Class for position and velocity measurement using a MT6701 magnetic
 *        encoder. This class starts its own measurement task at the specified
 *        frequency which reads the current angle, updates the accumulator, and
 *        filters / updates the velocity measurement.
 *
 * @note There is an implicit assumption in this class regarding the maximum
 *       velocity it can measure (above which there will be aliasing). The
 *       fastest velocity it can measure will be (0.5f * update_period * 60.0f)
 *       which is half a rotation in one update period.
 *
 * @note The assumption above also affects the reliability of the accumulator,
 *       since it is based on accumulating position differences every update
 *       period.
 *
 * \section mt6701_ex1 Mt6701 Example
 * \snippet mt6701_example.cpp mt6701 example
 */
class Mt6701 {
public:
  static constexpr uint8_t ADDRESS = (0b0000110); ///< I2C address of the MT6701

  /**
   * @brief Function to write a byte to a register for Mt6701.
   * @param reg_addr Register address to write to.
   * @param data Data to be written.
   */
  typedef std::function<void(uint8_t reg_addr, uint8_t data)> write_fn;

  /**
   * @brief Function to read a byte from a register for Mt6701.
   * @param reg_addr Register address to read from.
   * @return Byte read from register.
   */
  typedef std::function<uint8_t(uint8_t reg_addr)> read_fn;

  /**
   * @brief Filter the input raw velocity and return it.
   * @param raw Most recent raw velocity measured.
   * @return Filtered velocity.
   */
  typedef std::function<float(float raw)> velocity_filter_fn;

  static constexpr int   COUNTS_PER_REVOLUTION = 16384; ///< Int number of counts per revolution for the magnetic encoder.
  static constexpr float COUNTS_PER_REVOLUTION_F = 16384.0f; ///< Float number of counts per revolution for the magnetic encoder.
  static constexpr float COUNTS_TO_RADIANS = 2.0f * M_PI / COUNTS_PER_REVOLUTION_F; ///< Conversion factor to convert from count value to radians.
  static constexpr float COUNTS_TO_DEGREES = 360.0f / COUNTS_PER_REVOLUTION_F; ///< Conversion factor to convert from count value to degrees.
  static constexpr float SECONDS_PER_MINUTE = 60.0f; ///< Conversion factor to convert from seconds to minutes.

  /**
   * @brief Configuration information for the Mt6701.
   */
  struct Config {
    write_fn write; ///< Function to write to the device.
    read_fn read; ///< Function to read from the device.
    velocity_filter_fn velocity_filter; ///< Function to filter the veolcity. @note Will be called once every update_period seconds.
    std::chrono::duration<float> update_period{.01f}; ///< Update period (1/sample rate) in seconds. This determines the periodicity of the task which will read the position, update the accumulator, and update/filter velocity.
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  /**
   * @brief Construct the Mt6701 and start the update task.
   */
  Mt6701(const Config& config)
    : write_(config.write),
      read_(config.read),
      velocity_filter_(config.velocity_filter),
      update_period_(config.update_period),
      logger_({.tag = "Mt6701", .level = config.log_level}) {
    logger_.info("Initializing. Fastest measurable velocity will be {:.3f} RPM",
                 // half a rotation in one update period is the fastest we can
                 // measure
                 0.5f / update_period_.count() * SECONDS_PER_MINUTE);
    init();
  }

  /**
   * @brief Get the most recently updated raw count value from the encoder.
   * @note This value always represents the angle of the encoder modulo one
   *       rotation, meaning it only represents the range 0 to 360 degrees. It
   *       is not recommended to use this function, but is provided for edge use
   *       cases.
   * @return Raw count value in the range [0, 16384] (0 to 360 degrees).
   */
  int get_count() const {
    return count_.load();
  }

  /**
   * @brief Return the accumulated count that the encoder has generated since it
   *        was initialized.
   * @note This value is a raw counter value that can be +/-, meaning
   *       COUNTS_PER_REVOLUTION can be used to convert it to revolutions.
   * @return Raw accumulator value.
   */
  int get_accumulator() const {
    return accumulator_.load();
  }

  /**
   * @brief Return the accumulated position of the encoder, in radians.
   * @note This can be any value, it is not restricted to [-2pi, 2pi].
   * @return Position in radians of the encoder.
   */
  float get_radians() const {
    return (float)get_accumulator() * COUNTS_TO_RADIANS;
  }

  /**
   * @brief Return the accumulated position of the encoder, in degrees.
   * @note This can be any value, it is not restricted to [-360, 360].
   * @return Position in degrees of the encoder.
   */
  float get_degrees() const {
    return (float)get_accumulator() * COUNTS_TO_DEGREES;
  }

  /**
   * @brief Return the filtered velocity of the encoder, in RPM.
   * @return Filtered velocity (revolutions / minute, RPM).
   */
  float get_rpm() const {
    return velocity_rpm_.load();
  }

protected:
  int read_count() {
    logger_.info("read_count");
    // read the angle count registers
    uint8_t angle_h = read_((uint8_t)Registers::ANGLE_H);
    uint8_t angle_l = read_((uint8_t)Registers::ANGLE_L) >> 2;
    return (int)((angle_h << 6) | angle_l);
  }

  void update() {
    logger_.info("update");
    // update raw count
    count_ = read_count();
    static int prev_count = count_;
    // compute diff
    int diff = count_ - prev_count;
    // update prev_count
    prev_count = count_;
    // check for zero crossing
    if (diff > COUNTS_PER_REVOLUTION/2) {
      // we crossed zero going clockwise (1 -> 359)
      diff -= COUNTS_PER_REVOLUTION;
    } else if (diff < -COUNTS_PER_REVOLUTION/2) {
      // we crossed zero going counter-clockwise (359 -> 1)
      diff += COUNTS_PER_REVOLUTION;
    }
    // update accumulator
    accumulator_ += diff;
    logger_.debug("CDA: {}, {}, {}", count_, diff, accumulator_);
    // update velocity (filtering it)
    static auto prev_time = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float>(now - prev_time).count();
    prev_time = now;
    float seconds = elapsed ? elapsed : update_period_.count();
    float raw_velocity = (float)(diff) / COUNTS_PER_REVOLUTION_F / seconds * SECONDS_PER_MINUTE;
    velocity_rpm_ = velocity_filter_(raw_velocity);
    static float max_velocity = 0.5f / update_period_.count() * SECONDS_PER_MINUTE;
    if (raw_velocity >= max_velocity) {
      logger_.warn("Velocity nearing measurement limit ({:.3f} RPM), consider decreasing your update period!",
                   max_velocity);
    }
  }

  void update_task(std::mutex &m, std::condition_variable &cv) {
    auto start = std::chrono::high_resolution_clock::now();
    update();
    {
      std::unique_lock<std::mutex> lk(m);
      cv.wait_until(lk, start + update_period_);
    }
  }

  void init() {
    // initialize the accumulator to have the current angle
    accumulator_ = read_count();
    // start the task
    using namespace std::placeholders;
    task_ = espp::Task::make_unique({
        .name = "Mt6701",
        .callback = std::bind(&Mt6701::update_task, this, _1, _2)
      });
    task_->start();
  }

  /**
   * @brief Register map for the MT6701.
   *
   * @note The MT6701 contains a push-button output (pin 5) with configuration
   *       (mentioned on page 25) via PUSH_THRD register, PUSH_DIFF_DLY
   *       register, and PUSH_TIME_OUT register. However, the register addresses
   *       for these configurations (and their bitfields) are not provided in
   *       the datasheet and must be provided by the manufacturer.
   *
   * @note The push button can only be read from the MT6701 when using SSI
   *       communications, and is returned as part of the magnetic field status
   *       truth table (page 24).
   */
  enum class Registers : uint8_t {
    ANGLE_H = 0x03, ///< Angle[13:6]
    ANGLE_L = 0x04, ///< Angle[5:0] (bit 2-7)
    MUX_1   = 0x25, ///< UVW MUX (bit 7)
    MUX_2   = 0x29, ///< ABZ MUX (bit 6), DIR (bit 1)
    RES_1   = 0x30, ///< UVW_RES (bit 4-7), ABZ_RES[9:8] (bit 0-1)
    RES_2   = 0x31, ///< ABZ_RES[7:0]
    CONFIG_1= 0x32, ///< HYST[2] (bit 7), Z_PULSE_WIDTH (bit 4-6), ZERO[11:8] (bit 0-3)
    CONFIG_2= 0x33, ///< ZERO[7:0]
    CONFIG_3= 0x34, ///< HYST[1:0] (bit 6-7)
    CONFIG_4= 0x38, ///< PWM_FREQ (bit 7), PWM_POL (bit 6), OUT_MODE (bit 5)
    A_SS_HIGH   = 0x3E, ///< A_STOP[11:8] (bit 4-7), A_START[11:8] (bit 0-3)
    A_START_LOW = 0x3F, ///< A_START[7:0]
    A_STOP_LOW  = 0x40, ///< A_STOP[7:0]
  };

  write_fn write_;
  read_fn read_;
  velocity_filter_fn velocity_filter_{nullptr};
  std::chrono::duration<float> update_period_;
  std::atomic<int> count_{0};
  std::atomic<int> accumulator_{0};
  std::atomic<float> velocity_rpm_{0};
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
};
