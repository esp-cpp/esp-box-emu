#pragma once

#include <sdkconfig.h>

#include <esp_err.h>
#include <esp_partition.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <hal/usb_phy_types.h>
#include <esp_private/usb_phy.h>

#include <tinyusb.h>
#include <class/msc/msc.h>
#include <tusb_msc_storage.h>

#include "esp-box.hpp"
#include "event_manager.hpp"

#include "aw9523.hpp"
#include "base_component.hpp"
#include "button.hpp"
#include "drv2605.hpp"
#include "events.hpp"
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

#include "make_color.h"

class BoxEmu : public espp::BaseComponent {
public:

  // Define the BSP class for easier access, and potential ability to change the
  // BSP if we want to support other targets.
  using Bsp = espp::EspBox;

  // Wrap some of the EspBox defines / methods for easier access and to remove
  // dependency on EspBox from other components
  using button_callback_t = Bsp::button_callback_t;
  using Pixel = Bsp::Pixel;
  using DisplayDriver = Bsp::DisplayDriver;
  using TouchpadData = Bsp::TouchpadData;
  using touch_callback_t = Bsp::touch_callback_t;

  // Wrap some of the EspBox defines / methods for easier access and to remove
  // dependency on EspBox from other components
  static constexpr size_t lcd_width() { return Bsp::lcd_width(); }
  static constexpr size_t lcd_height() { return Bsp::lcd_height(); }
  Bsp::Pixel *vram0() const { return Bsp::get().vram0(); }
  Bsp::Pixel *vram1() const { return Bsp::get().vram1(); }
  uint8_t *frame_buffer0() const { return Bsp::get().frame_buffer0(); }
  uint8_t *frame_buffer1() const { return Bsp::get().frame_buffer1(); }
  void brightness(float v){ Bsp::get().brightness(v); }
  float brightness() const { return Bsp::get().brightness(); }
  bool is_muted() const { return Bsp::get().is_muted(); }
  void mute(bool v) { Bsp::get().mute(v); }
  void volume(float volume) { Bsp::get().volume(volume); }
  float volume() const { return Bsp::get().volume(); }
  void audio_sample_rate(int rate) { Bsp::get().audio_sample_rate(rate); }
  uint32_t audio_sample_rate() const { return Bsp::get().audio_sample_rate(); }
  void play_audio(const uint8_t *data, size_t size) { Bsp::get().play_audio(data, size); }
  void play_audio(const std::vector<uint8_t> &data) { Bsp::get().play_audio(data); }

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

  BoxEmu(const BoxEmu &) = delete;
  BoxEmu &operator=(const BoxEmu &) = delete;
  BoxEmu(BoxEmu &&) = delete;
  BoxEmu &operator=(BoxEmu &&) = delete;

  static constexpr char mount_point[] = "/sdcard";

  /// Get the version of the BoxEmu that was detected
  /// \return The version of the BoxEmu that was detected
  /// \see Version
  Version version() const;

  /// Get a reference to the internal I2C bus
  /// \return A reference to the internal I2C bus
  /// \note The internal I2C bus is used for the touchscreen and audio codec
  espp::I2c &internal_i2c();

  /// Get a reference to the external I2C bus
  /// \return A reference to the external I2C bus
  /// \note The external I2C bus is used for the gamepad functionality
  espp::I2c &external_i2c();

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
  size_t copy_file_to_romdata(const std::string& filename);
  uint8_t *romdata() const;

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
  std::shared_ptr<espp::Max1704x> battery() const;

  /////////////////////////////////////////////////////////////////////////////
  // Video
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_video();
  void clear_screen();
  void display_size(size_t width, size_t height);
  void native_size(size_t width, size_t height, int pitch = -1);
  const uint16_t *palette() const;
  void palette(const uint16_t *palette, size_t size = 256);
  void push_frame(const void* frame);
  VideoSetting video_setting() const;
  void video_setting(const VideoSetting setting);

  /////////////////////////////////////////////////////////////////////////////
  // Haptic Motor (DRV2605)
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_haptics();
  std::shared_ptr<espp::Drv2605> haptics() const;
  void play_haptic_effect();
  void play_haptic_effect(int effect);
  void set_haptic_effect(int effect);

  /////////////////////////////////////////////////////////////////////////////
  // USB
  /////////////////////////////////////////////////////////////////////////////

  bool initialize_usb();
  bool deinitialize_usb();
  bool is_usb_enabled() const;

protected:
  BoxEmu();
  void detect();

  bool has_palette() const;
  bool is_native() const;
  int x_offset() const;
  int y_offset() const;
  bool video_task_callback(std::mutex &m, std::condition_variable &cv, bool &task_notified);

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
        auto &box = Bsp::get();
        float current_volume = box.volume();
        float new_volume = std::clamp<float>(current_volume + volume_change, 0, 100);
        box.volume(new_volume);
        // send out a volume change event
        espp::EventManager::get().publish(volume_changed_topic, {});
      }
    }
  protected:
    std::shared_ptr<InputDriver> input_driver;
  };

  struct version0 {
    using InputDriver = espp::Mcp23x17;
    typedef Input<version0, InputDriver> InputType;
    static constexpr auto input_address = InputDriver::DEFAULT_ADDRESS;
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
    typedef Input<version1, InputDriver> InputType;
    static constexpr auto input_address = InputDriver::DEFAULT_ADDRESS;
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

  // external I2c (peripherals)
  static constexpr auto external_i2c_port = I2C_NUM_1;
  static constexpr auto external_i2c_clock_speed = 400 * 1000;
  static constexpr gpio_num_t external_i2c_sda = GPIO_NUM_41;
  static constexpr gpio_num_t external_i2c_scl = GPIO_NUM_40;

  // uSD card
  static constexpr gpio_num_t sdcard_cs = GPIO_NUM_10;
  static constexpr gpio_num_t sdcard_mosi = GPIO_NUM_11;
  static constexpr gpio_num_t sdcard_miso = GPIO_NUM_13;
  static constexpr gpio_num_t sdcard_sclk = GPIO_NUM_12;
  static constexpr auto sdcard_spi_num = SPI3_HOST;

  static constexpr int num_rows_in_framebuffer = 30;

  Version version_{Version::UNKNOWN};

  espp::I2c external_i2c_{{.port = external_i2c_port,
        .sda_io_num = external_i2c_sda,
        .scl_io_num = external_i2c_scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE}};

  // sdcard
  sdmmc_card_t *sdcard_{nullptr};

  // memory
  uint8_t *romdata_{nullptr};

  // audio
  std::shared_ptr<espp::Button> mute_button_;

  // gamepad
  std::atomic<bool> can_read_gamepad_{true};
  std::recursive_mutex gamepad_state_mutex_;
  GamepadState gamepad_state_;
  std::shared_ptr<InputBase> input_;
  std::shared_ptr<espp::KeypadInput> keypad_;
  std::shared_ptr<espp::HighResolutionTimer> input_timer_;

  // battery
  std::atomic<bool> battery_comms_good_{true};
  std::shared_ptr<espp::Max1704x> battery_{nullptr};
  std::shared_ptr<espp::OneshotAdc> adc_{nullptr};
  std::shared_ptr<espp::HighResolutionTimer> battery_task_;
  std::vector<espp::AdcConfig> channels;

  // video
  std::atomic<VideoSetting> video_setting_{VideoSetting::FIT};
  std::unique_ptr<espp::Task> video_task_{nullptr};
  QueueHandle_t video_queue_{nullptr};

  size_t display_width_{Bsp::lcd_width()};
  size_t display_height_{Bsp::lcd_height()};

  size_t native_width_{Bsp::lcd_width()};
  size_t native_height_{Bsp::lcd_height()};
  int native_pitch_{Bsp::lcd_width()};

  const uint16_t* palette_{nullptr};
  size_t palette_size_{256};

  // haptics
  std::shared_ptr<espp::Drv2605> haptic_motor_{nullptr};

  // usb
  std::atomic<bool> usb_enabled_{false};
  usb_phy_handle_t jtag_phy_;
};

// for libfmt printing of the BoxEmu::Version enum
template <>
struct fmt::formatter<BoxEmu::Version> : fmt::formatter<std::string> {
  template <typename FormatContext>
  auto format(BoxEmu::Version v, FormatContext &ctx) const {
    std::string_view name;
    switch (v) {
      case BoxEmu::Version::UNKNOWN:
        name = "UNKNOWN";
        break;
      case BoxEmu::Version::V0:
        name = "V0";
        break;
      case BoxEmu::Version::V1:
        name = "V1";
        break;
    }
    return fmt::formatter<std::string>::format(name, ctx);
  }
};
