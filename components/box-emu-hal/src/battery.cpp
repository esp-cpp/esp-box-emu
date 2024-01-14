#include "battery.hpp"

static std::shared_ptr<espp::Max1704x> battery_;
static std::unique_ptr<espp::Task> battery_task_;
static bool battery_initialized_ = false;

using namespace std::chrono_literals;

void hal::battery_init() {
  if (battery_initialized_) {
    return;
  }
  auto i2c = hal::get_external_i2c();
  battery_ = std::make_shared<espp::Max1704x>(espp::Max1704x::Config{
      .device_address = espp::Max1704x::DEFAULT_ADDRESS,
      .write = std::bind(&espp::I2c::write, i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      .read = std::bind(&espp::I2c::read, i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    });
  battery_task_ = std::make_unique<espp::Task>(espp::Task::Config{
      .name = "battery",
        .callback = [](auto &m, auto &cv) {
          // sleep up here so we can easily early return below
          {
            std::unique_lock<std::mutex> lk(m);
            cv.wait_for(lk, 100ms);
          }
          // get the state of charge (%)
          std::error_code ec;
          auto soc = battery_->get_battery_percentage(ec);
          if (ec) {
            return false;
          }
          // get the charge rate (+/- % per hour)
          auto charge_rate = battery_->get_battery_charge_rate(ec);
          if (ec) {
            return false;
          }
          // now publish a BatteryInfo struct to the battery_topic
          auto battery_info = BatteryInfo{
            .level = soc,
            .charge_rate = charge_rate,
          };
          std::vector<uint8_t> battery_info_data;
          auto bytes_serialized = espp::serialize(battery_info, battery_info_data);
          if (bytes_serialized == 0) {
            return false;
          }
          espp::EventManager::get().publish(battery_topic, battery_info_data);
          return false;
        },
        .stack_size_bytes = 3 * 1024});
}

std::shared_ptr<espp::Max1704x> hal::get_battery() {
  battery_init();
  return battery_;
}
