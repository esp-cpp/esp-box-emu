#include "ads1x15.hpp"

static constexpr uint16_t REG_CONFIG_OS_SINGLE =
  (0x8000);
static constexpr uint16_t REG_CONFIG_OS_BUSY =
  (0x0000); ///< Read: Bit = 0 when conversion is in progress
static constexpr uint16_t REG_CONFIG_OS_NOTBUSY =
  (0x8000); ///< Read: Bit = 1 when device is not performing a conversion

static constexpr uint16_t REG_CONFIG_MUX_DIFF_0_1 =
  (0x0000); ///< Differential P = AIN0, N = AIN1 (default)
static constexpr uint16_t REG_CONFIG_MUX_DIFF_0_3 =
  (0x1000); ///< Differential P = AIN0, N = AIN3
static constexpr uint16_t REG_CONFIG_MUX_DIFF_1_3 =
  (0x2000); ///< Differential P = AIN1, N = AIN3
static constexpr uint16_t REG_CONFIG_MUX_DIFF_2_3 =
  (0x3000); ///< Differential P = AIN2, N = AIN3
static constexpr uint16_t REG_CONFIG_MUX_SINGLE_0 = (0x4000); ///< Single-ended AIN0
static constexpr uint16_t REG_CONFIG_MUX_SINGLE_1 = (0x5000); ///< Single-ended AIN1
static constexpr uint16_t REG_CONFIG_MUX_SINGLE_2 = (0x6000); ///< Single-ended AIN2
static constexpr uint16_t REG_CONFIG_MUX_SINGLE_3 = (0x7000); ///< Single-ended AIN3

static constexpr uint16_t MUX_BY_CHANNEL[] = {
  REG_CONFIG_MUX_SINGLE_0, ///< Single-ended AIN0
  REG_CONFIG_MUX_SINGLE_1, ///< Single-ended AIN1
  REG_CONFIG_MUX_SINGLE_2, ///< Single-ended AIN2
  REG_CONFIG_MUX_SINGLE_3  ///< Single-ended AIN3
};                                   ///< MUX config by channel

static constexpr uint16_t REG_CONFIG_MODE_CONTIN = (0x0000); ///< Continuous conversion mode
static constexpr uint16_t REG_CONFIG_MODE_SINGLE =
  (0x0100); ///< Power-down single-shot mode (default)

static constexpr uint16_t REG_CONFIG_CMODE_TRAD =
  (0x0000); ///< Traditional comparator with hysteresis (default)
static constexpr uint16_t REG_CONFIG_CMODE_WINDOW = (0x0010); ///< Window comparator

static constexpr uint16_t REG_CONFIG_CPOL_ACTVLOW =
  (0x0000); ///< ALERT/RDY pin is low when active (default)
static constexpr uint16_t REG_CONFIG_CPOL_ACTVHI =
  (0x0008); ///< ALERT/RDY pin is high when active

static constexpr uint16_t REG_CONFIG_CLAT_NONLAT =
  (0x0000); ///< Non-latching comparator (default)
static constexpr uint16_t REG_CONFIG_CLAT_LATCH = (0x0004); ///< Latching comparator

static constexpr uint16_t REG_CONFIG_CQUE_1CONV =
  (0x0000); ///< Assert ALERT/RDY after one conversions
static constexpr uint16_t REG_CONFIG_CQUE_2CONV =
  (0x0001); ///< Assert ALERT/RDY after two conversions
static constexpr uint16_t REG_CONFIG_CQUE_4CONV =
  (0x0002); ///< Assert ALERT/RDY after four conversions
static constexpr uint16_t REG_CONFIG_CQUE_NONE =
  (0x0003); ///< Disable the comparator and put ALERT/RDY in high state (default)

int16_t Ads1x15::sample_raw(int channel) {
  if (!write_ || !read_) {
    logger_.error("Write / read functions not properly configured, cannot sample!");
    return 0;
  }
  // Start with default values
  uint16_t config =
      REG_CONFIG_CQUE_1CONV |   // Comparator enabled and asserts on 1
                                        // match
      REG_CONFIG_CLAT_NONLAT |   // non-latching (default val)
      REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
      REG_CONFIG_CMODE_TRAD |   // Traditional comparator (default val)
      REG_CONFIG_MODE_SINGLE;   // Single conversion mode
  // Set PGA/voltage range
  config |= (uint16_t)gain_;
  // Set data rate
  config |= rate_;
  config |= MUX_BY_CHANNEL[channel];
  // Set 'start single-conversion' bit
  config |= REG_CONFIG_OS_SINGLE;
  // configure to read from mux 0
  logger_.debug("configuring conversion for channel {}", channel);
  write_((uint8_t)Register::POINTER_CONFIG, config);
  write_((uint8_t)Register::POINTER_HITHRESH, 0x8000);
  write_((uint8_t)Register::POINTER_LOWTHRESH, 0x0000);
  // wait for conversion complete
  logger_.debug("waiting for conversion complete...");
  while (!conversion_complete()) {
    vTaskDelay(1);
  }
  logger_.debug("reading conversion result for channel {}", channel);
  uint16_t val = read_((uint8_t)Register::POINTER_CONVERT) >> bit_shift_;
  if (bit_shift_ > 0) {
    if (val > 0x07FF) {
      // negative number - extend the sign to the 16th bit
      val |= 0xF000;
    }
  }
  return (int16_t)val;
}

bool Ads1x15::conversion_complete() {
  return (read_((uint8_t)Register::POINTER_CONFIG) & 0x8000) != 0;
}
