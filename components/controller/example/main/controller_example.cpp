#include <chrono>
#include <vector>

#include "driver/i2c.h"

#include "ads1x15.hpp"
#include "controller.hpp"
#include "oneshot_adc.hpp"
#include "task.hpp"

using namespace std::chrono_literals;

#define I2C_NUM         (I2C_NUM_1)
#define I2C_SCL_IO      (GPIO_NUM_40)
#define I2C_SDA_IO      (GPIO_NUM_41)
#define I2C_FREQ_HZ     (400 * 1000)
#define I2C_TIMEOUT_MS  (10)

extern "C" void app_main(void) {
  // First example shows using 2 analog pins with the controller for the x/y
  // joystick
  {
    std::atomic<bool> quit_test = false;
    fmt::print("Starting oneshot adc joystick controller example, press start & select together to quit!\n");
    //! [oneshot adc joystick controller example]
    // make the adc we'll be reading from
    std::vector<espp::AdcConfig> channels{
      {
        .unit = ADC_UNIT_2,
        .channel = ADC_CHANNEL_1, // (x) Analog 0 on the joystick shield
        .attenuation = ADC_ATTEN_DB_11
      },
      {
        .unit = ADC_UNIT_2,
        .channel = ADC_CHANNEL_2, // (y) Analog 1 on the joystick shield
        .attenuation = ADC_ATTEN_DB_11
      }
    };
    espp::OneshotAdc adc(espp::OneshotAdc::Config{
        .unit = ADC_UNIT_2,
        .channels = channels,
      });
    // make the function which will get the raw data from the ADC and convert to
    // uncalibrated [-1,1]
    auto read_joystick = [&adc, &channels](float *x, float *y) -> bool {
      fmt::print("reading joystick...\n");
      auto maybe_x_mv = adc.read_mv(channels[0].channel);
      auto maybe_y_mv = adc.read_mv(channels[1].channel);
      if (maybe_x_mv.has_value() && maybe_y_mv.has_value()) {
        auto x_mv = maybe_x_mv.value();
        auto y_mv = maybe_y_mv.value();
        fmt::print("got joystick values: {}mV, {}mV\n", x_mv, y_mv);
        *x = (x_mv / 1700.0f - 1.0f);
        *y = (y_mv / 1700.0f - 1.0f);
        fmt::print("converted: {}, {}\n", *x, *y);
        return true;
      }
      return false;
    };
    // make the controller - NOTE: this was designed for connecting the Sparkfun
    // Joystick Shield to the ESP32 S3 BOX
    Controller controller(Controller::AnalogJoystickConfig{
        // buttons short to ground, so they are active low. this will enable the
        // GPIO_PULLUP and invert the logic
        .active_low = true,
        .gpio_a = 38, // D3 on the joystick shield
        .gpio_b = 39, // D5 on the joystick shield
        .gpio_x = -1, // we're using this as start...
        .gpio_y = -1, // we're using this as select...
        .gpio_start = 42,  // D4 on the joystick shield
        .gpio_select = 21, // D6 on the joystick shield
        .gpio_joystick_select = -1, // D2 on the joystick shield
        .joystick_config = {
          .x_calibration = {.center = 0.0f, .deadband = 0.2f, .minimum = -1.0f, .maximum = 1.0f},
          .y_calibration = {.center = 0.0f, .deadband = 0.2f, .minimum = -1.0f, .maximum = 1.0f},
          .get_values = read_joystick,
          .log_level = espp::Logger::Verbosity::WARN
        },
        .log_level = espp::Logger::Verbosity::WARN
      });
    // and finally, make the task to periodically poll the controller and print
    // the state
    auto task_fn = [&quit_test, &controller](std::mutex& m, std::condition_variable& cv) {
      controller.update();
      bool is_a_pressed = controller.is_pressed(Controller::Button::A);
      bool is_b_pressed = controller.is_pressed(Controller::Button::B);
      bool is_select_pressed = controller.is_pressed(Controller::Button::SELECT);
      bool is_start_pressed = controller.is_pressed(Controller::Button::START);
      bool is_up_pressed = controller.is_pressed(Controller::Button::UP);
      bool is_down_pressed = controller.is_pressed(Controller::Button::DOWN);
      bool is_left_pressed = controller.is_pressed(Controller::Button::LEFT);
      bool is_right_pressed = controller.is_pressed(Controller::Button::RIGHT);
      fmt::print("Controller buttons:\n"
                 "\tA:      {}\n"
                 "\tB:      {}\n"
                 "\tSelect: {}\n"
                 "\tStart:  {}\n"
                 "\tUp:     {}\n"
                 "\tDown:   {}\n"
                 "\tLeft:   {}\n"
                 "\tRight:  {}\n",
                 is_a_pressed,
                 is_b_pressed,
                 is_select_pressed,
                 is_start_pressed,
                 is_up_pressed,
                 is_down_pressed,
                 is_left_pressed,
                 is_right_pressed
                 );
      quit_test = is_start_pressed && is_select_pressed;
      // NOTE: sleeping in this way allows the sleep to exit early when the
      // task is being stopped / destroyed
      {
        std::unique_lock<std::mutex> lk(m);
        cv.wait_for(lk, 500ms);
      }
    };
    auto task = espp::Task({
        .name = "Controller Task",
        .callback = task_fn,
        .stack_size_bytes = 6*1024,
        .log_level = espp::Logger::Verbosity::WARN
      });
    task.start();
    //! [oneshot adc joystick controller example]
    while (!quit_test) {
      std::this_thread::sleep_for(100ms);
    }
  }

  std::this_thread::sleep_for(500ms);

  // Second example shows using the i2c adc (ads1x15) with the controller for
  // the x/y joystick
  {
    std::atomic<bool> quit_test = false;
    fmt::print("Starting i2c adc joystick controller example, press start & select together to quit!\n");
    //! [i2c adc joystick controller example]
    // make the I2C that we'll use to communicate
    i2c_config_t i2c_cfg;
    fmt::print("initializing i2c driver...\n");
    memset(&i2c_cfg, 0, sizeof(i2c_cfg));
    i2c_cfg.sda_io_num = I2C_SDA_IO; // pin 3 on the joybonnet
    i2c_cfg.scl_io_num = I2C_SCL_IO; // pin 5 on the joybonnet
    i2c_cfg.mode = I2C_MODE_MASTER;
    i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_cfg.master.clk_speed = I2C_FREQ_HZ;
    auto err = i2c_param_config(I2C_NUM, &i2c_cfg);
    if (err != ESP_OK) printf("config i2c failed\n");
    err = i2c_driver_install(I2C_NUM, I2C_MODE_MASTER,  0, 0, 0);
    if (err != ESP_OK) printf("install i2c driver failed\n");
    // make some lambda functions we'll use to read/write to the i2c adc
    auto ads_write = [](uint8_t reg_addr, uint16_t value) {
      uint8_t write_buf[3] = {reg_addr, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
      i2c_master_write_to_device(I2C_NUM,
                                 Ads1x15::ADDRESS,
                                 write_buf,
                                 3,
                                 I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    };
    auto ads_read = [](uint8_t reg_addr) -> uint16_t {
      uint8_t read_data[2];
      i2c_master_write_read_device(I2C_NUM,
                                   Ads1x15::ADDRESS,
                                   &reg_addr,
                                   1, // size of addr
                                   read_data,
                                   2,
                                   I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
      return (read_data[0] << 8) | read_data[1];
    };
    // make the actual ads class
    Ads1x15 ads(Ads1x15::Ads1015Config{
        .write = ads_write,
        .read = ads_read
      });
    // make the task which will get the raw data from the I2C ADC and convert to
    // uncalibrated [-1,1]
    std::atomic<float> joystick_x{0};
    std::atomic<float> joystick_y{0};
    auto ads_read_task_fn = [&joystick_x, &joystick_y, &ads](std::mutex& m, std::condition_variable& cv) {
      // NOTE: sleeping in this way allows the sleep to exit early when the
      // task is being stopped / destroyed
      {
        using namespace std::chrono_literals;
        std::unique_lock<std::mutex> lk(m);
        cv.wait_for(lk, 20ms);
      }
      auto x_mv = ads.sample_mv(1);
      auto y_mv = ads.sample_mv(0);
      joystick_x.store(x_mv / 1700.0f - 1.0f);
      // y is inverted so negate it
      joystick_y.store(-(y_mv / 1700.0f - 1.0f));
    };
    auto ads_task = espp::Task::make_unique({
        .name = "ADS",
        .callback = ads_read_task_fn,
        .stack_size_bytes{4*1024},
        .log_level = espp::Logger::Verbosity::INFO
      });
    ads_task->start();
    // make the read joystick function used by the controller
    auto read_joystick = [&joystick_x, &joystick_y](float *x, float *y) -> bool {
      *x = joystick_x.load();
      *y = joystick_y.load();
      return true;
    };
    // make the controller - NOTE: this was designed for connecting the Adafruit
    // JoyBonnet to the ESP32 S3 BOX
    Controller controller(Controller::AnalogJoystickConfig{
        // buttons short to ground, so they are active low. this will enable the
        // GPIO_PULLUP and invert the logic
        .active_low = true,
        .gpio_a = 38, // pin 32 on the joybonnet
        .gpio_b = 39, // pin 31 on the joybonnet
        .gpio_x = -1, // pin 36 on the joybonnet
        .gpio_y = -1, // pin 33 on the joybonnet
        .gpio_start = 42,  // pin 37 on the joybonnet
        .gpio_select = 21, // pin 38 on the joybonnet
        .gpio_joystick_select = -1,
        .joystick_config = {
          .x_calibration = {.center = 0.0f, .deadband = 0.2f, .minimum = -1.0f, .maximum = 1.0f},
          .y_calibration = {.center = 0.0f, .deadband = 0.2f, .minimum = -1.0f, .maximum = 1.0f},
          .get_values = read_joystick,
          .log_level = espp::Logger::Verbosity::WARN
        },
        .log_level = espp::Logger::Verbosity::WARN
      });
    // and finally, make the task to periodically poll the controller and print
    // the state
    auto task_fn = [&quit_test, &controller](std::mutex& m, std::condition_variable& cv) {
      controller.update();
      bool is_a_pressed = controller.is_pressed(Controller::Button::A);
      bool is_b_pressed = controller.is_pressed(Controller::Button::B);
      bool is_select_pressed = controller.is_pressed(Controller::Button::SELECT);
      bool is_start_pressed = controller.is_pressed(Controller::Button::START);
      bool is_up_pressed = controller.is_pressed(Controller::Button::UP);
      bool is_down_pressed = controller.is_pressed(Controller::Button::DOWN);
      bool is_left_pressed = controller.is_pressed(Controller::Button::LEFT);
      bool is_right_pressed = controller.is_pressed(Controller::Button::RIGHT);
      fmt::print("Controller buttons:\n"
                 "\tA:      {}\n"
                 "\tB:      {}\n"
                 "\tSelect: {}\n"
                 "\tStart:  {}\n"
                 "\tUp:     {}\n"
                 "\tDown:   {}\n"
                 "\tLeft:   {}\n"
                 "\tRight:  {}\n",
                 is_a_pressed,
                 is_b_pressed,
                 is_select_pressed,
                 is_start_pressed,
                 is_up_pressed,
                 is_down_pressed,
                 is_left_pressed,
                 is_right_pressed
                 );
      quit_test = is_start_pressed && is_select_pressed;
      // NOTE: sleeping in this way allows the sleep to exit early when the
      // task is being stopped / destroyed
      {
        std::unique_lock<std::mutex> lk(m);
        cv.wait_for(lk, 500ms);
      }
    };
    auto task = espp::Task({
        .name = "Controller Task",
        .callback = task_fn,
        .stack_size_bytes = 6*1024,
        .log_level = espp::Logger::Verbosity::WARN
      });
    task.start();
    //! [i2c adc joystick controller example]
    while (!quit_test) {
      std::this_thread::sleep_for(100ms);
    }
  }
  // now clean up the i2c driver (by now the task will have stopped, because we
  // left its scope.
  i2c_driver_delete(I2C_NUM);

  fmt::print("Controller example complete!\n");

  while (true) {
    std::this_thread::sleep_for(1s);
  }
}
