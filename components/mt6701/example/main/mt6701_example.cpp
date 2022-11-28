#include <chrono>
#include <vector>

#include "driver/i2c.h"

#include "butterworth_filter.hpp"
#include "task.hpp"
#include "mt6701.hpp"

using namespace std::chrono_literals;

#define I2C_NUM         (I2C_NUM_1)
#define I2C_SCL_IO      (GPIO_NUM_40)
#define I2C_SDA_IO      (GPIO_NUM_41)
#define I2C_FREQ_HZ     (400 * 1000)
#define I2C_TIMEOUT_MS  (10)

extern "C" void app_main(void) {
  {
    std::atomic<bool> quit_test = false;
    fmt::print("Starting mt6701 example, rotate to -720 degrees to quit!\n");
    //! [mt6701 example]
    // make the I2C that we'll use to communicate
    i2c_config_t i2c_cfg;
    fmt::print("initializing i2c driver...\n");
    memset(&i2c_cfg, 0, sizeof(i2c_cfg));
    i2c_cfg.sda_io_num = I2C_SDA_IO;
    i2c_cfg.scl_io_num = I2C_SCL_IO;
    i2c_cfg.mode = I2C_MODE_MASTER;
    i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_cfg.master.clk_speed = I2C_FREQ_HZ;
    auto err = i2c_param_config(I2C_NUM, &i2c_cfg);
    if (err != ESP_OK) printf("config i2c failed\n");
    err = i2c_driver_install(I2C_NUM, I2C_MODE_MASTER,  0, 0, 0);
    if (err != ESP_OK) printf("install i2c driver failed\n");
    // make some lambda functions we'll use to read/write to the mt6701
    auto mt6701_write = [](uint8_t reg_addr, uint8_t value) {
      uint8_t data[] = {reg_addr, value};
      i2c_master_write_to_device(I2C_NUM,
                                 Mt6701::ADDRESS,
                                 data,
                                 2,
                                 I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    };

    auto mt6701_read = [](uint8_t reg_addr) -> uint8_t{
      uint8_t data;
      i2c_master_write_read_device(I2C_NUM,
                                   Mt6701::ADDRESS,
                                   &reg_addr,
                                   1,
                                   &data,
                                   1,
                                   I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
      return data;
    };
    // make the velocity filter
    static constexpr float filter_cutoff_hz = 4.0f;
    static constexpr float encoder_update_period = 0.01f; // seconds
    espp::ButterworthFilter<2, espp::BiquadFilterDf2> filter({
        .normalized_cutoff_frequency = 2.0f * filter_cutoff_hz * encoder_update_period
      });
    auto filter_fn = [&filter](float raw) -> float {
      return filter.update(raw);
    };
    // now make the mt6701 which decodes the data
    Mt6701 mt6701({
        .write = mt6701_write,
        .read = mt6701_read,
        .velocity_filter = filter_fn,
        .update_period = std::chrono::duration<float>(encoder_update_period),
        .log_level = espp::Logger::Verbosity::WARN
      });
    // and finally, make the task to periodically poll the mt6701 and print the
    // state. NOTE: the Mt6701 runs its own task to maintain state, so we're
    // just polling the current state.
    auto task_fn = [&quit_test, &mt6701](std::mutex& m, std::condition_variable& cv) {
      static auto start = std::chrono::high_resolution_clock::now();
      auto now = std::chrono::high_resolution_clock::now();
      auto seconds = std::chrono::duration<float>(now-start).count();
      auto count = mt6701.get_count();
      auto radians = mt6701.get_radians();
      auto degrees = mt6701.get_degrees();
      auto rpm = mt6701.get_rpm();
      fmt::print("{:.3f}, {}, {:.3f}, {:.3f}, {:.3f}\n",
                 seconds,
                 count,
                 radians,
                 degrees,
                 rpm
                 );
      quit_test = degrees <= -720.0f;
      // NOTE: sleeping in this way allows the sleep to exit early when the
      // task is being stopped / destroyed
      {
        std::unique_lock<std::mutex> lk(m);
        cv.wait_for(lk, 50ms);
      }
    };
    auto task = espp::Task({
        .name = "Mt6701 Task",
        .callback = task_fn,
        .stack_size_bytes = 5*1024,
        .log_level = espp::Logger::Verbosity::WARN
      });
    fmt::print("%time(s), count, radians, degrees, rpm\n");
    task.start();
    //! [mt6701 example]
    while (!quit_test) {
      std::this_thread::sleep_for(100ms);
    }
    // now clean up the i2c driver
    i2c_driver_delete(I2C_NUM);
  }

  fmt::print("Mt6701 example complete!\n");

  while (true) {
    std::this_thread::sleep_for(1s);
  }
}
