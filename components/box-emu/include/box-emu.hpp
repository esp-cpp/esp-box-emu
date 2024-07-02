#pragma once

#include <sdkconfig.h>

#include <esp_err.h>
#include <nvs_flash.h>
#include <spi_flash_mmap.h>
#include <esp_partition.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <hal/usb_phy_types.h>
#include <esp_private/usb_phy.h>

#include <tinyusb.h>
#include <tusb_msc_storage.h>

#include "esp-box.hpp"

#include "aw9523.hpp"
#include "base_component.hpp"
#include "high_resolution_timer.hpp"
#include "keypad_input.hpp"
#include "max1704x.hpp"
#include "mcp23x17.hpp"
#include "oneshot_adc.hpp"
#include "serialization.hpp"
#include "task.hpp"
#include "timer.hpp"

#include "battery_info.hpp"
#include "gamepad_state.hpp"
#include "video_setting.hpp"

class BoxEmu : public espp::BaseComponent {
public:
  /// The Version of the BoxEmu
  enum class Version {
    UNKNOWN, ///< unknown box
    V0,      ///< first version of the box
    V1,      ///< second version of the box
  };

  /// @brief Access the singleton instance of the BoxEmu class
  /// @return Reference to the singleton instance of the BoxEmu class
  static BoxEmu &get() {
    static BoxEmu instance;
    return instance;
  }

  BoxEmu(const BoxEMu &) = delete;
  BoxEmu &operator=(const BoxEmu &) = delete;
  BoxEmu(BoxEmu &&) = delete;
  BoxEmu &operator=(BoxEmu &&) = delete;

  /// Get the version of the BoxEmu that was detected
  /// \return The version of the BoxEmu that was detected
  /// \see Version
  Version version() const;

  /// Get a reference to the internal I2C bus
  /// \return A reference to the internal I2C bus
  /// \note The internal I2C bus is used for the touchscreen and audio codec
  I2c &internal_i2c();

  /// Get a reference to the external I2C bus
  /// \return A reference to the external I2C bus
  /// \note The external I2C bus is used for the gamepad functionality
  I2c &external_i2c();

  /// Initialize the EspBox hardware
  /// \return True if the initialization was successful, false otherwise
  /// \note This initializes the touch, display, and sound subsystems which are
  ///       internal to the EspBox
  /// \see EspBox
  bool initialize_box();

  /////////////////////////////////////////////////////////////////////////////
  // uSD Card
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_sdcard();
  sdmmc_card_t *sdcard() const;

  /////////////////////////////////////////////////////////////////////////////
  // Memory
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_memory();
  size_t copy_romdata_to_cart_partition(const std::string& filename);
  uint8_t *mapped_romdata() const;

  /////////////////////////////////////////////////////////////////////////////
  // Gamepad
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_gamepad();
  bool update_gamepad_state();
  GamepadState gamepad_state();
  void keypad_read(bool *up, bool *down, bool *left, bool *right, bool *enter, bool *escape);
  std::shared_ptr<espp::KeypadInput> keypad() const;

  /////////////////////////////////////////////////////////////////////////////
  // Battery
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_battery();
  std::shared_ptr<espp::Max1704x> battery();

  /////////////////////////////////////////////////////////////////////////////
  // Video
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_video();
  void set_display_size(size_t width, size_t height);
  void set_native_size(size_t width, size_t height, int pitch = -1);
  void set_palette(const uint16_t *palette, size_t size = 256);
  void push_frame(const void* frame);
  VideoSetting video_setting() const;
  void video_setting(VideoSetting setting);

  /////////////////////////////////////////////////////////////////////////////
  // USB
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_usb();
  bool deinitialize_usb();
  bool is_usb_enabled() const;

protected:
  BoxEmu();
  void detect();

  struct version0 {
    using InputDriver = espp::mcp23x17;
    static constexpr uint16_t START_PIN =  (1<<0) << 0; // start pin is on port a of the MCP23x17
    static constexpr uint16_t SELECT_PIN = (1<<1) << 0; // select pin is on port a of the MCP23x17
    static constexpr uint16_t UP_PIN =    (1<<0) << 8; // up pin is on port b of the MCP23x17
    static constexpr uint16_t DOWN_PIN =  (1<<1) << 8; // down pin is on port b of the MCP23x17
    static constexpr uint16_t LEFT_PIN =  (1<<2) << 8; // left pin is on port b of the MCP23x17
    static constexpr uint16_t RIGHT_PIN = (1<<3) << 8; // right pin is on port b of the MCP23x17
    static constexpr uint16_t A_PIN =     (1<<4) << 8; // a pin is on port b of the MCP23x17
    static constexpr uint16_t B_PIN =     (1<<5) << 8; // b pin is on port b of the MCP23x17
    static constexpr uint16_t X_PIN =     (1<<6) << 8; // x pin is on port b of the MCP23x17
    static constexpr uint16_t Y_PIN =     (1<<7) << 8; // y pin is on port b of the MCP23x17
    static constexpr uint16_t BAT_ALERT_PIN = 0; // battery alert pin doesn't exist on the MCP23x17
    static constexpr uint16_t VOL_UP_PIN =    0; // volume up pin doesn't exist on the MCP23x17
    static constexpr uint16_t VOL_DOWN_PIN =  0; // volume down pin doesn't exist on the MCP23x17
    static constexpr uint16_t DIRECTION_MASK = (UP_PIN | DOWN_PIN | LEFT_PIN | RIGHT_PIN | A_PIN | B_PIN | X_PIN | Y_PIN | START_PIN | SELECT_PIN);
    static constexpr uint16_t INTERRUPT_MASK = (START_PIN | SELECT_PIN);
    static constexpr uint16_t INVERT_MASK = (UP_PIN | DOWN_PIN | LEFT_PIN | RIGHT_PIN | A_PIN | B_PIN | X_PIN | Y_PIN | START_PIN | SELECT_PIN ); // pins are active low so invert them
    static constexpr uint8_t PORT_0_DIRECTION_MASK = DIRECTION_MASK & 0xFF;
    static constexpr uint8_t PORT_1_DIRECTION_MASK = (DIRECTION_MASK >> 8) & 0xFF;
    static constexpr uint8_t PORT_0_INTERRUPT_MASK = INTERRUPT_MASK & 0xFF;
    static constexpr uint8_t PORT_1_INTERRUPT_MASK = (INTERRUPT_MASK >> 8) & 0xFF;
  };

  struct version1 {
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

  class InputBase {
  public:
    virtual uint16_t get_pins(std::error_code& ec) = 0;
    virtual GamepadState pins_to_gamepad_state(uint16_t pins) = 0;
    virtual void handle_volume_pins(uint16_t pins) = 0;
  };

  template <typename T, typename InputDriver>
  class Input : public InputBase {
  public:
    explicit Input(std::shared_ptr<InputDriver> input_driver) : input_driver(input_driver) {}
    virtual uint16_t get_pins(std::error_code& ec) override {
      auto val = input_driver->get_pins(ec);
      if (ec) {
        return 0;
      }
      return val ^ T::INVERT_MASK;
    }
    virtual GamepadState pins_to_gamepad_state(uint16_t pins) override {
      GamepadState state;
      state.a = (bool)(pins & T::A_PIN);
      state.b = (bool)(pins & T::B_PIN);
      state.x = (bool)(pins & T::X_PIN);
      state.y = (bool)(pins & T::Y_PIN);
      state.start = (bool)(pins & T::START_PIN);
      state.select = (bool)(pins & T::SELECT_PIN);
      state.up = (bool)(pins & T::UP_PIN);
      state.down = (bool)(pins & T::DOWN_PIN);
      state.left = (bool)(pins & T::LEFT_PIN);
      state.right = (bool)(pins & T::RIGHT_PIN);
      return state;
    }
    virtual void handle_volume_pins(uint16_t pins) override {
      // check the volume pins and send out events if they're pressed / released
      bool volume_up = (bool)(pins & T::VOL_UP_PIN);
      bool volume_down = (bool)(pins & T::VOL_DOWN_PIN);
      int volume_change = (volume_up * 10) + (volume_down * -10);
      if (volume_change != 0) {
        // change the volume
        float current_volume = espp::EspBox::volume();
        float new_volume = std::clamp<float>(current_volume + volume_change, 0, 100);
        espp::EspBox::volume(new_volume);
        // send out a volume change event
        espp::EventManager::get().publish(volume_changed_topic, {});
      }
    }
  protected:
    std::shared_ptr<InputDriver> input_driver;
  };

  // external I2c (peripherals)
  static constexpr auto external_i2c_port = I2C_NUM_1;
  static constexpr auto external_i2c_clock_speed = 400 * 1000;
  static constexpr gpio_num_t external_i2c_sda = GPIO_NUM_41;
  static constexpr gpio_num_t external_i2c_scl = GPIO_NUM_40;

  Version version_{Version::UNKNOWN};

  I2c external_i2c_{{.port = external_i2c_port,
                     .sda_io_num = external_i2c_sda,
                     .scl_io_num = external_i2c_scl,
                     .sda_pullup_en = GPIO_PULLUP_ENABLE,
                     .scl_pullup_en = GPIO_PULLUP_ENABLE}};

  std::recursive_mutex gamepad_state_mutex_;
  GamepadState gamepad_state_;
  std::shared_ptr<InputBase> input_;
  std::shared_ptr<espp::KeypadInput> keypad_;
  std::shared_ptr<espp::HighResolutionTimer> input_timer_;
};
