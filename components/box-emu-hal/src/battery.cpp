#include "box_emu_hal.hpp"

static std::shared_ptr<espp::Max1704x> battery_{nullptr};
static std::shared_ptr<espp::OneshotAdc> adc_{nullptr};
static std::unique_ptr<espp::Task> battery_task_;
static bool battery_initialized_ = false;
static std::vector<espp::AdcConfig> channels;

using namespace std::chrono_literals;

void hal::battery_init() {
  if (battery_initialized_) {
    return;
  }
  fmt::print("Initializing battery...\n");
  auto i2c = hal::get_external_i2c();
  bool can_communicate = i2c->probe_device(espp::Max1704x::DEFAULT_ADDRESS);
  if (!can_communicate) {
    fmt::print("Could not communicate with battery!\n");
    // go ahead and set the battery_initialized_ flag to true so we don't try to
    // initialize the battery again
    battery_initialized_ = true;
    return;
  }

  // // NOTE: we could also make an ADC for measuring battery voltage
  // // make the adc channels
  // channels.clear();
  // channels.push_back({
  //     .unit = BATTERY_ADC_UNIT,
  //     .channel = BATTERY_ADC_CHANNEL,
  //     .attenuation = ADC_ATTEN_DB_12});
  // adc_ = std::make_shared<espp::OneshotAdc>(espp::OneshotAdc::Config{
  //     .unit = BATTERY_ADC_UNIT,
  //     .channels = channels
  //   });

  // now make the Max17048 that we'll use to get good state of charge, charge
  // rate, etc.
  battery_ = std::make_shared<espp::Max1704x>(espp::Max1704x::Config{
      .device_address = espp::Max1704x::DEFAULT_ADDRESS,
      .write = std::bind(&espp::I2c::write, i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      .read = std::bind(&espp::I2c::read, i2c.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    });
  // NOTE: the MAX17048 is tied to the VBAT for its power supply (as you would
  // imagine), this means that we cannnot communicate with it if the battery is
  // not connected. Therefore, if we are unable to communicate with the battery
  // we will just return and not start the battery task.
  battery_task_ = std::make_unique<espp::Task>(espp::Task::Config{
      .name = "battery",
        .callback = [](auto &m, auto &cv) {
          // sleep up here so we can easily early return below
          {
            std::unique_lock<std::mutex> lk(m);
            cv.wait_for(lk, 500ms);
          }
          std::error_code ec;
          // get the voltage (V)
          auto voltage = battery_->get_battery_voltage(ec);
          if (ec) {
            return false;
          }
          // get the state of charge (%)
          auto soc = battery_->get_battery_percentage(ec);
          if (ec) {
            return false;
          }
          // get the charge rate (+/- % per hour)
          auto charge_rate = battery_->get_battery_charge_rate(ec);
          if (ec) {
            return false;
          }

          // NOTE: we could also get voltage from the adc for the battery if we
          //       wanted, but the MAX17048 gives us the same voltage anyway
          // auto maybe_mv = adc_->read_mv(channels[0]);
          // if (maybe_mv.has_value()) {
          //   // convert mv -> V and from the voltage divider (R1=R2) to real
          //   // battery volts
          //   voltage = maybe_mv.value() / 1000.0f * 2.0f;
          // }

          // now publish a BatteryInfo struct to the battery_topic
          auto battery_info = BatteryInfo{
            .voltage = voltage,
            .level = soc,
            .charge_rate = charge_rate,
          };
          std::vector<uint8_t> battery_info_data;
          // fmt::print("Publishing battery info: {}\n", battery_info);
          auto bytes_serialized = espp::serialize(battery_info, battery_info_data);
          if (bytes_serialized == 0) {
            return false;
          }
          espp::EventManager::get().publish(battery_topic, battery_info_data);
          return false;
        },
        .stack_size_bytes = 4 * 1024});
  battery_task_->start();
  battery_initialized_ = true;
}

std::shared_ptr<espp::Max1704x> hal::get_battery() {
  battery_init();
  return battery_;
}
