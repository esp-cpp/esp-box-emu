#include <chrono>
#include <vector>

#include "driver/i2c.h"

#include "task.hpp"
#include "qwiicnes.hpp"

using namespace std::chrono_literals;

#define I2C_NUM         (I2C_NUM_1)
#define I2C_SCL_IO      (GPIO_NUM_40)
#define I2C_SDA_IO      (GPIO_NUM_41)
#define I2C_FREQ_HZ     (400 * 1000)
#define I2C_TIMEOUT_MS  (10)

extern "C" void app_main(void) {
  {
    std::atomic<bool> quit_test = false;
    fmt::print("Starting qwiicnes example, press select & start together to quit!\n");
    //! [qwiicnes example]
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
    // make some lambda functions we'll use to read/write to the qwiicnes
    auto qwiicnes_write = [](uint8_t reg_addr, uint8_t value) {
      uint8_t data[] = {reg_addr, value};
      i2c_master_write_to_device(I2C_NUM,
                                 QwiicNes::ADDRESS,
                                 data,
                                 2,
                                 I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    };

    auto qwiicnes_read = [](uint8_t reg_addr) -> uint8_t{
      uint8_t data;
      i2c_master_write_read_device(I2C_NUM,
                                   QwiicNes::ADDRESS,
                                   &reg_addr,
                                   1,
                                   &data,
                                   1,
                                   I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
      return data;
    };
    // now make the qwiicnes which decodes the data
    QwiicNes qwiicnes({
        .write = qwiicnes_write,
        .read = qwiicnes_read,
        .log_level = espp::Logger::Verbosity::WARN
      });
    // and finally, make the task to periodically poll the qwiicnes and print
    // the state
    auto task_fn = [&quit_test, &qwiicnes](std::mutex& m, std::condition_variable& cv) {
      qwiicnes.update();
      bool is_a_pressed = qwiicnes.is_pressed(QwiicNes::Button::A);
      bool is_b_pressed = qwiicnes.is_pressed(QwiicNes::Button::B);
      bool is_select_pressed = qwiicnes.is_pressed(QwiicNes::Button::SELECT);
      bool is_start_pressed = qwiicnes.is_pressed(QwiicNes::Button::START);
      bool is_up_pressed = qwiicnes.is_pressed(QwiicNes::Button::UP);
      bool is_down_pressed = qwiicnes.is_pressed(QwiicNes::Button::DOWN);
      bool is_left_pressed = qwiicnes.is_pressed(QwiicNes::Button::LEFT);
      bool is_right_pressed = qwiicnes.is_pressed(QwiicNes::Button::RIGHT);
      fmt::print("QwiicNes buttons:\n"
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
      quit_test = is_select_pressed && is_start_pressed;
      // NOTE: sleeping in this way allows the sleep to exit early when the
      // task is being stopped / destroyed
      {
        std::unique_lock<std::mutex> lk(m);
        cv.wait_for(lk, 500ms);
      }
    };
    auto task = espp::Task({
        .name = "Qwiicnes Task",
        .callback = task_fn,
        .log_level = espp::Logger::Verbosity::WARN
      });
    task.start();
    //! [qwiicnes example]
    while (!quit_test) {
      std::this_thread::sleep_for(100ms);
    }
    // now clean up the i2c driver
    i2c_driver_delete(I2C_NUM);
  }

  fmt::print("Qwiicnes example complete!\n");

  while (true) {
    std::this_thread::sleep_for(1s);
  }
}
