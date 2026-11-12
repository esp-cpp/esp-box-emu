#include "box-emu.hpp"

#include <hal/usb_serial_jtag_ll.h>
#include <soc/rtc_cntl_struct.h>

#include <cstdio>
#include <cstring>
#include <esp_cpu.h>
#include <soc/spi_struct.h>

BoxEmu::BoxEmu() : espp::BaseComponent("BoxEmu") {
  detect();
}

BoxEmu::Version BoxEmu::version() const {
  return version_;
}

void BoxEmu::detect() {
  bool version0_found = external_i2c_.probe_device(version0::input_address);
  bool version1_found = external_i2c_.probe_device(version1::input_address);
  if (version1_found) {
    // Version 1
    version_ = BoxEmu::Version::V1;
  } else if (version0_found) {
    // Version 0
    version_ = BoxEmu::Version::V0;
  } else {
    logger_.warn("No box-emu hardware detected");
    logger_.warn("\tProbed for MCP23x17 (version0) at 0x{:02x}", version0::input_address);
    logger_.warn("\tProbed for AW9523 (version1) at 0x{:02x}", version1::input_address);
    // No box detected
    version_ = BoxEmu::Version::UNKNOWN;
    return;
  }
  logger_.info("version {}", version_);
}

espp::I2c &BoxEmu::internal_i2c() {
  return Bsp::get().internal_i2c();
}

espp::I2c &BoxEmu::external_i2c() {
  return external_i2c_;
}

bool BoxEmu::initialize_box() {
  logger_.info("Initializing underlying BSP");
  auto &box = Bsp::get();
  // initialize the sound
  // The I2S feed task does almost no work (hand one block to the DMA) but it
  // must never be starved: it shares core 1 with the emulators' sound and
  // video tasks (priority 20), and at the BSP's default priority 19 it only
  // ran in the gaps between frames -- when core 1 was saturated (Sonic 3's
  // special stages) the DMA underran and the audio queue overflowed, dropping
  // ~7% of the audio. Above both emulator tasks it always runs promptly.
  if (!box.initialize_sound(48000, {.name = "audio", .stack_size_bytes = 4096, .priority = 21, .core_id = 1})) {
    logger_.error("Failed to initialize sound!");
    return false;
  }
  // initialize the LCD
  if (!box.initialize_lcd()) {
    logger_.error("Failed to initialize LCD!");
    return false;
  }
  {
    // Report the clock the LCD SPI device actually got. The SPI master can only
    // divide its 80MHz source by integers (80, 40, 26.7, ...), so a request like
    // 60MHz silently becomes 40MHz -- and that wire rate is the display's frame
    // rate ceiling (320x240x16bpp = 30.7ms at 40MHz).
    // (The device's clock register is only loaded on the first transaction, so
    // this is read after the panel init sequence has run.)
    const uint32_t pre = GPSPI2.clock.clkdiv_pre;
    const uint32_t n = GPSPI2.clock.clkcnt_n;
    const uint32_t equ = GPSPI2.clock.clk_equ_sysclk;
    const uint32_t src_hz = 80 * 1000 * 1000;
    const uint32_t spi_hz = equ ? src_hz : src_hz / ((pre + 1) * (n + 1));
    fmt::print("[display] lcd spi clock: {} Hz (pre={} n={} equ_sysclk={})\n", spi_hz, pre, n, equ);
  }
  static constexpr size_t pixel_buffer_size = Bsp::lcd_width() * num_rows_in_framebuffer;
  // initialize the LVGL display for the esp-box
  if (!box.initialize_display(pixel_buffer_size)) {
    logger_.error("Failed to initialize display!");
    return false;
  }
  // initialize the touchpad
  if (!box.initialize_touch()) {
    logger_.error("Failed to initialize touchpad!");
    return false;
  }

  // initialize the mute button to broadcast the mute state
  logger_.info("Initializing mute button");
  mute_button_ = std::make_shared<espp::Button>(espp::Button::Config{
      .name = "mute button",
      .interrupt_config =
          {
              .gpio_num = Bsp::get_mute_pin(),
              .callback =
                  [](const espp::Interrupt::Event &event) {
                    Bsp::get().mute(event.active);
                    // simply publish that the mute button was presssed
                    espp::EventManager::get().publish(mute_button_topic, {});
                  },
              .active_level = espp::Interrupt::ActiveLevel::LOW,
              .interrupt_type = espp::Interrupt::Type::ANY_EDGE,
              .pullup_enabled = true,
              .pulldown_enabled = false,
          },
      .task_config =
          {
              .name = "mute button task",
              .stack_size_bytes = 4 * 1024,
              .priority = 5,
          },
      .log_level = espp::Logger::Verbosity::WARN,
  });

  // update the mute state (since it's a flip-flop and may have been set if we
  // restarted without power loss)
  Bsp::get().mute(mute_button_->is_pressed());
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// uSD Card
/////////////////////////////////////////////////////////////////////////////

bool BoxEmu::initialize_sdcard() {
  if (sdcard_) {
    logger_.error("SD card already initialized!");
    return false;
  }

  logger_.info("Initializing SD card");

  // Native SD (SDMMC) mode on the same lines older firmware drove in SPI
  // mode: CLK=SCK, CMD=MOSI, DAT0=MISO, DAT3=CS. The ESP32-S3 routes the
  // SDMMC host through the GPIO matrix, so it runs on these pins and roughly
  // triples throughput versus the 20MHz SPI ceiling. DAT3/DAT1/DAT2 are held
  // high by on-board pullups (DAT3 high also keeps the card out of SPI mode).
  espp::SdCard::SdmmcConfig sdmmc;
  sdmmc.bus_width = sdcard_bus_width;
  sdmmc.clk = sdcard_clk;
  sdmmc.cmd = sdcard_cmd;
  sdmmc.d0 = sdcard_d0;
  if constexpr (sdcard_bus_width == 4) {
    sdmmc.d1 = sdcard_d1;
    sdmmc.d2 = sdcard_d2;
    sdmmc.d3 = sdcard_d3;
  }
  sdmmc.frequency_khz = SDMMC_FREQ_HIGHSPEED;

  espp::SdCard::Config config;
  config.interface = sdmmc;
  config.mount_point = mount_point;
  config.mount_on_initialize = true;
  config.format_if_mount_failed = false;
  // Dark Forces (TFE) keeps its GOB and LFD archives open while running; allow enough handles.
  config.max_files = sdcard_max_files;
  config.allocation_unit_size = 2 * 1024;
  config.log_level = espp::Logger::Verbosity::INFO;

  auto sdcard = std::make_unique<espp::SdCard>(config);
  std::error_code ec;
  if (!sdcard->initialize(ec)) {
    logger_.error("Failed to initialize / mount the SD card: {}. "
                  "Make sure a FAT formatted card is inserted.", ec.message());
    return false;
  }
  sdcard_ = std::move(sdcard);

  logger_.info("Filesystem mounted");

  // Card has been initialized, print its properties
  sdcard_->print_info(stdout);

  return true;
}

sdmmc_card_t *BoxEmu::sdcard() const {
  return sdcard_ ? sdcard_->card() : nullptr;
}

espp::SdCard *BoxEmu::sdcard_component() const {
  return sdcard_.get();
}

/////////////////////////////////////////////////////////////////////////////
// Memory
/////////////////////////////////////////////////////////////////////////////

static constexpr size_t memory_size = 4*1024*1024;

extern "C" uint8_t *osd_getromdata() {
  auto &emu = BoxEmu::get();
  return emu.romdata();
}

bool BoxEmu::initialize_memory() {
  if (romdata_) {
    logger_.error("ROM already initialized!");
    return false;
  }

  logger_.info("Initializing memory (romdata)");
  // allocate memory for the ROM and make sure it's on the SPIRAM
  romdata_ = (uint8_t*)heap_caps_malloc(memory_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  if (romdata_ == nullptr) {
    logger_.error("Couldn't allocate memory for ROM!");
    return false;
  }

  return true;
}

void BoxEmu::deinitialize_memory() {
  if (romdata_) {
    logger_.info("Deinitializing memory (romdata)");
    free(romdata_);
    romdata_ = nullptr;
  }
}

size_t BoxEmu::copy_file_to_romdata(const std::string& filename) {
  // load the file data and iteratively copy it over
  std::ifstream romfile(filename, std::ios::binary | std::ios::ate); //open file at end
  if (!romfile.is_open()) {
    logger_.error("ROM file does not exist");
    return 0;
  }
  size_t filesize = romfile.tellg(); // get size from current file pointer location;
  romfile.seekg(0, std::ios::beg); //reset file pointer to beginning;
  romfile.read((char*)(romdata_), filesize);
  romfile.close();

  return filesize;
}

uint8_t *BoxEmu::romdata() const {
  return romdata_;
}

/////////////////////////////////////////////////////////////////////////////
// Gamepad
/////////////////////////////////////////////////////////////////////////////

bool BoxEmu::initialize_gamepad() {
  logger_.info("Initializing gamepad");
  if (version_ == BoxEmu::Version::V0) {
    auto raw_input = new version0::InputType(
                                             std::make_shared<version0::InputDriver>(version0::InputDriver::Config{
                                                 .device_address = version0::input_address,
                                                 .port_0_direction_mask = version0::PORT_0_DIRECTION_MASK,
                                                 .port_0_interrupt_mask = version0::PORT_0_INTERRUPT_MASK,
                                                 .port_1_direction_mask = version0::PORT_1_DIRECTION_MASK,
                                                 .port_1_interrupt_mask = version0::PORT_1_INTERRUPT_MASK,
                                                 .write = std::bind(&espp::I2c::write, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                                                 .read_register = std::bind(&espp::I2c::read_at_register, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
                                                 .log_level = espp::Logger::Verbosity::WARN
                                               })
                                             );
    input_.reset(raw_input);
  } else if (version_ == BoxEmu::Version::V1) {
    auto raw_input = new version1::InputType(
                                             std::make_shared<version1::InputDriver>(version1::InputDriver::Config{
                                                 .device_address = version1::input_address,
                                                 .port_0_direction_mask = version1::PORT_0_DIRECTION_MASK,
                                                 .port_0_interrupt_mask = version1::PORT_0_INTERRUPT_MASK,
                                                 .port_1_direction_mask = version1::PORT_1_DIRECTION_MASK,
                                                 .port_1_interrupt_mask = version1::PORT_1_INTERRUPT_MASK,
                                                 .write = std::bind(&espp::I2c::write, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                                                 .write_then_read = std::bind(&espp::I2c::write_read, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5),
                                                 .log_level = espp::Logger::Verbosity::WARN
                                               })
                                             );
    input_.reset(raw_input);
  } else {
    return false;
  }

  // now initialize the keypad driver
  keypad_ = std::make_shared<espp::KeypadInput>(espp::KeypadInput::Config{
      .read = std::bind(&BoxEmu::keypad_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6),
      .log_level = espp::Logger::Verbosity::WARN
    });

  // now initialize the input timer
  input_timer_ = std::make_shared<espp::HighResolutionTimer>(espp::HighResolutionTimer::Config{
      .name = "Input timer",
      .callback = [this]() {
        update_gamepad_state();
      }});
  uint64_t period_us = 30 * 1000;
  input_timer_->periodic(period_us);

  return true;
}

GamepadState BoxEmu::gamepad_state() {
  std::lock_guard<std::recursive_mutex> lock(gamepad_state_mutex_);
  GamepadState state = gamepad_state_;
  // buttons held by the USB link's test control (scripted input)
  state.buttons |= remote_buttons_;
  return state;
}

void BoxEmu::set_remote_buttons(uint16_t buttons) {
  std::lock_guard<std::recursive_mutex> lock(gamepad_state_mutex_);
  gamepad_press_latch_ |= (uint16_t)(buttons & ~remote_buttons_);
  remote_buttons_ = buttons;
}

bool BoxEmu::update_gamepad_state() {
  if (!input_) {
    return false;
  }
  if (!can_read_gamepad_) {
    return false;
  }
  std::error_code ec;
  auto pins = input_->get_pins(ec);
  if (ec) {
    logger_.error("Error reading input pins: {}", ec.message());
    can_read_gamepad_ = false;
    return false;
  }

  auto new_gamepad_state = input_->pins_to_gamepad_state(pins);
  bool changed = false;
  {
    std::lock_guard<std::recursive_mutex> lock(gamepad_state_mutex_);
    changed = gamepad_state_ != new_gamepad_state;
    // remember rising edges for gamepad_state_latched()
    gamepad_press_latch_ |= (uint16_t)(new_gamepad_state.buttons & ~gamepad_state_.buttons);
    gamepad_state_ = new_gamepad_state;
  }
  input_->handle_volume_pins(pins);

  return changed;
}

GamepadState BoxEmu::gamepad_state_latched() {
  std::lock_guard<std::recursive_mutex> lock(gamepad_state_mutex_);
  GamepadState state = gamepad_state_;
  state.buttons |= gamepad_press_latch_ | remote_buttons_;
  gamepad_press_latch_ = 0;
  return state;
}

void BoxEmu::keypad_read(bool *up, bool *down, bool *left, bool *right, bool *enter, bool *escape) {
  const GamepadState state = gamepad_state();
  *up = state.up;
  *down = state.down;
  *left = state.left;
  *right = state.right;

  *enter = state.a || state.start;
  *escape = state.b || state.select;
}

std::shared_ptr<espp::KeypadInput> BoxEmu::keypad() const {
  return keypad_;
}

/////////////////////////////////////////////////////////////////////////////
// Battery
/////////////////////////////////////////////////////////////////////////////

bool BoxEmu::initialize_battery() {
  if (version_ == BoxEmu::Version::V0) {
    logger_.warn("Battery not supported on version 0");
    return false;
  }

  if (battery_) {
    logger_.error("Battery already initialized!");
    return false;
  }

  logger_.info("Initializing battery");

  battery_comms_good_ = external_i2c_.probe_device(espp::Max1704x::DEFAULT_ADDRESS);
  if (!battery_comms_good_) {
    logger_.error("Could not communicate with battery!");
    return false;
  }

  // now make the Max17048 that we'll use to get good state of charge, charge
  // rate, etc.
  battery_ = std::make_shared<espp::Max1704x>(espp::Max1704x::Config{
      .device_address = espp::Max1704x::DEFAULT_ADDRESS,
      .write = std::bind(&espp::I2c::write, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      .read = std::bind(&espp::I2c::read, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    });

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

  // NOTE: the MAX17048 is tied to the VBAT for its power supply (as you would
  // imagine), this means that we cannnot communicate with it if the battery is
  // not connected. Therefore, if we are unable to communicate with the battery
  // we will just return and not start the battery task.
  battery_task_ = std::make_shared<espp::HighResolutionTimer>(espp::HighResolutionTimer::Config{
      .name = "battery",
        .callback = [this]() {
          std::error_code ec;
          // get the voltage (V)
          auto voltage = battery_->get_battery_voltage(ec);
          if (ec) {
            fmt::print("Error getting battery voltage: {}\n", ec.message());
            fmt::print("Battery is probably not connected!\n");
            fmt::print("Stopping battery task...\n");
            battery_task_->stop();
            return;
          }
          // get the state of charge (%)
          auto soc = battery_->get_battery_percentage(ec);
          if (ec) {
            fmt::print("Error getting battery percentage: {}\n", ec.message());
            fmt::print("Battery is probably not connected!\n");
            fmt::print("Stopping battery task...\n");
            battery_task_->stop();
            return;
          }
          // get the charge rate (+/- % per hour)
          auto charge_rate = battery_->get_battery_charge_rate(ec);
          if (ec) {
            fmt::print("Error getting battery charge rate: {}\n", ec.message());
            fmt::print("Battery is probably not connected!\n");
            fmt::print("Stopping battery task...\n");
            battery_task_->stop();
            return;
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
            return;
          }
          espp::EventManager::get().publish(battery_topic, battery_info_data);
          return;
        }});
  uint64_t battery_period_us = 500 * 1000; // 500ms
  battery_task_->periodic(battery_period_us);

  return true;
}

std::shared_ptr<espp::Max1704x> BoxEmu::battery() const {
  return battery_;
}

/////////////////////////////////////////////////////////////////////////////
// Video
/////////////////////////////////////////////////////////////////////////////

bool BoxEmu::initialize_video() {
  if (video_task_) {
    logger_.error("Video task already initialized!");
    return false;
  }

  logger_.info("initializing video task");

  video_queue_ = xQueueCreate(1, sizeof(uint16_t*));
  using namespace std::placeholders;
  video_task_ = espp::Task::make_unique({
      .callback = std::bind(&BoxEmu::video_task_callback, this, _1, _2, _3),
      .task_config = {
        .name = "video task",
        .stack_size_bytes = 4*1024,
        .priority = 20,
        .core_id = 1
      },
    });
  video_task_->start();

  return true;
}

void BoxEmu::clear_screen() {
  static int buffer = 0;
  xQueueSend(video_queue_, &buffer, 10);
}

void BoxEmu::display_size(size_t width, size_t height) {
  display_width_ = width;
  display_height_ = height;
}

void BoxEmu::native_size(size_t width, size_t height, int pitch) {
  native_width_ = width;
  native_height_ = height;
  native_pitch_ = pitch == -1 ? width : pitch;
}

void BoxEmu::palette(const uint16_t *palette, size_t size) {
  palette_ = palette;
  palette_size_ = size;
}

void BoxEmu::push_frame(const void* frame) {
  if (video_queue_ == nullptr) {
    logger_.error("video queue is null, make sure to call initialize_video() first!");
    return;
  }
  if (xQueueSend(video_queue_, &frame, 0) != pdTRUE) {
    frames_dropped_.fetch_add(1, std::memory_order_relaxed);
  }
}

bool BoxEmu::frame_slot_free() const {
  return video_queue_ == nullptr || uxQueueMessagesWaiting(video_queue_) == 0;
}

VideoSetting BoxEmu::video_setting() const {
  return video_setting_;
}

void BoxEmu::video_setting(const VideoSetting setting) {
  video_setting_ = setting;
}

/////////////////////////////////////////////////////////////////////////////
// Haptic Motor
/////////////////////////////////////////////////////////////////////////////

bool BoxEmu::initialize_haptics() {
  if (haptic_motor_) {
    logger_.error("Haptics already initialized!");
    return false;
  }
  logger_.info("Initializing haptics");
  haptic_motor_ = std::make_shared<espp::Drv2605>(espp::Drv2605::Config{
      .device_address = espp::Drv2605::DEFAULT_ADDRESS,
      .write = std::bind(&espp::I2c::write, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      .read_register = std::bind(&espp::I2c::read_at_register, &external_i2c_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
      .motor_type = espp::Drv2605::MotorType::LRA
    });
  // we're using an LRA motor, so select th LRA library.
  std::error_code ec;
  haptic_motor_->select_library(espp::Drv2605::Library::LRA, ec);
  if (ec) {
    logger_.error("Error selecting LRA library: {}", ec.message());
    return false;
  }
  return true;
}

std::shared_ptr<espp::Drv2605> BoxEmu::haptics() const {
  return haptic_motor_;
}

void BoxEmu::play_haptic_effect() {
  if (haptic_motor_ == nullptr) {
    logger_.error("Haptic motor not initialized!");
    return;
  }
    std::error_code ec;
    haptic_motor_->start(ec);
    if (ec) {
      logger_.error("Error starting haptic motor: {}", ec.message());
    }
}

void BoxEmu::play_haptic_effect(int effect) {
  if (haptic_motor_ == nullptr) {
    logger_.error("Haptic motor not initialized!");
    return;
  }
  set_haptic_effect(effect);
  play_haptic_effect();
}

void BoxEmu::set_haptic_effect(int effect) {
  if (haptic_motor_ == nullptr) {
    logger_.error("Haptic motor not initialized!");
    return;
  }
  std::error_code ec;
  haptic_motor_->set_waveform(0, (espp::Drv2605::Waveform)(effect), ec);
  haptic_motor_->set_waveform(1, espp::Drv2605::Waveform::END, ec);
}

/////////////////////////////////////////////////////////////////////////////
// USB
/////////////////////////////////////////////////////////////////////////////

bool BoxEmu::initialize_usb_device() {
  if (usb_device_) {
    return true;
  }
  logger_.info("USB composite device initialization");
  auto card = sdcard();
  std::error_code ec;
  // the MSC function mounts the card for us while we own it: release our mount
  if (card && !sdcard_->unmount(ec)) {
    logger_.error("Could not unmount the SD card for the drive function: {}", ec.message());
    return false;
  }
  // The USB-Serial-JTAG console and USB-OTG share the PHY; release it so the
  // USB device stack can take it.
  if (jtag_phy_) {
    usb_del_phy(jtag_phy_);
    jtag_phy_ = nullptr;
  }

  espp::UsbDevice::Config config;
  // espp::UsbDevice's default (pid.codes) identity, so the stock espp tools
  // (espp_ota, espp_coredump, the web consoles) and tools/emulink find it
  config.manufacturer = "esp-cpp";
  config.product = "ESP-Box-Emu";
  config.serial_number = "esp-box-emu";
  config.max_power_ma = 500;
  config.log_level = espp::Logger::Verbosity::WARN;
  // the console: logs on this CDC port (teed to UART0)
  espp::UsbDevice::CdcFunction cdc;
  cdc.interface_name = "ESP-Box-Emu console";
  cdc.route_console = true;
  config.cdc = cdc;
  // the framed services (OTA, core dump, system, test control)
  espp::UsbDevice::VendorFunction vendor;
  vendor.interface_name = "ESP-Box-Emu link";
  vendor.webusb = true;
  vendor.landing_page_url = "esp-cpp.github.io/espp/apps/device_hub.html";
  vendor.on_receive = [this](std::span<const uint8_t> data) {
    if (vendor_rx_) vendor_rx_(data);
  };
  config.vendor = vendor;
  // the uSD card as a USB drive: ours until handed over (initialize_usb())
  if (card) {
    espp::UsbDevice::MscMedium medium;
    medium.type = espp::UsbDevice::MscMedium::Type::SdCard;
    medium.sd_card = card;
    medium.base_path = mount_point;
    medium.max_files = sdcard_max_files;
    medium.initial_owner = espp::UsbDevice::MscOwner::App;
    espp::UsbDevice::MscFunction msc;
    msc.interface_name = "ESP-Box-Emu uSD Card";
    msc.media = {medium};
    msc.auto_handover = false; // only when asked (settings screen / host request)
    msc.on_event = [this](size_t lun, espp::UsbDevice::MscEvent event, espp::UsbDevice::MscOwner owner) {
      if (event == espp::UsbDevice::MscEvent::OwnerChanged) {
        logger_.info("uSD card now owned by the {}",
                     owner == espp::UsbDevice::MscOwner::App ? "application" : "USB host");
      } else if (event == espp::UsbDevice::MscEvent::FormatRequired) {
        logger_.warn("uSD card has no filesystem; format it from the host");
      }
    };
    config.msc = msc;
  }

  auto usb = std::make_unique<espp::UsbDevice>(config);
  if (!usb->initialize(ec)) {
    logger_.error("Failed to initialize the USB device: {}", ec.message());
    usb.reset();
    if (card) sdcard_->mount();
    return false;
  }
  usb_device_ = std::move(usb);
  logger_.info("USB composite device up (console, link{})", card ? ", uSD drive" : "");
  // espp routes only stdout to the CDC console; send stderr (the cores' error
  // messages) there too. /dev/usbcons is the console device espp registers.
  if (usb_device_->is_console_routed_to_cdc() && freopen("/dev/usbcons", "w", stderr)) {
    setvbuf(stderr, nullptr, _IONBF, 0);
  }
  return true;
}

bool BoxEmu::vendor_write(std::span<const uint8_t> frame) {
  std::lock_guard<std::mutex> lk(vendor_tx_mutex_);
  return usb_device_ && usb_device_->write_vendor(frame);
}

void BoxEmu::usb_prepare_for_rom_download() {
  // The S3 has one internal USB PHY; the RTC-domain mux that hands it to the
  // OTG controller (TinyUSB) survives the reset, so the ROM's download mode
  // would come up on USB-Serial-JTAG with no PHY: detach, then give the PHY
  // back to USB-Serial-JTAG (and the mux to the eFuse default).
  if (usb_device_) {
    usb_device_->disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  usb_serial_jtag_ll_phy_enable_external(false);
  RTCCNTL.usb_conf.sw_usb_phy_sel = 0;
  RTCCNTL.usb_conf.sw_hw_usb_phy_sel = 0;
}

bool BoxEmu::initialize_usb() {
  if (!usb_device_ || usb_device_->msc_lun_count() == 0) {
    logger_.error("No uSD drive function");
    return false;
  }
  // the launcher reads the card (metadata, boxart) under the LVGL lock: hold
  // it so the volume is not unmounted under one of its reads
  std::lock_guard<std::recursive_mutex> lk(lvgl_mutex());
  if (before_sd_handover_) before_sd_handover_();
  std::error_code ec;
  if (!usb_device_->set_msc_owner(0, espp::UsbDevice::MscOwner::Host, ec)) {
    logger_.error("Could not hand the uSD card to the USB host: {}", ec.message());
    return false;
  }
  return true;
}

bool BoxEmu::deinitialize_usb() {
  if (!usb_device_ || usb_device_->msc_lun_count() == 0) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lk(lvgl_mutex()); // see initialize_usb()
  std::error_code ec;
  // The PC stays attached (console, link), so this is taken on trust that it
  // ejected the drive first (the settings screen says so; emulink drive off
  // ejects it): one that did not may lose writes it had cached, as on an unplug.
  if (!usb_device_->take_msc_from_ejected_host(0, ec)) {
    logger_.error("Could not take the uSD card back: {}", ec.message());
    return false;
  }
  return true;
}

bool BoxEmu::is_usb_enabled() const {
  if (!usb_device_ || usb_device_->msc_lun_count() == 0) {
    return false;
  }
  const auto owner = usb_device_->msc_owner(0);
  return owner.has_value() && *owner == espp::UsbDevice::MscOwner::Host;
}

/////////////////////////////////////////////////////////////////////////////
// Static Video Task:
/////////////////////////////////////////////////////////////////////////////

bool BoxEmu::has_palette() const {
  return palette_ != nullptr;
}

bool BoxEmu::is_native() const {
  return native_width_ == display_width_ && native_height_ == display_height_;
}

int BoxEmu::x_offset() const {
  return (Bsp::lcd_width()-display_width_)/2;
}

int BoxEmu::y_offset() const {
  return (Bsp::lcd_height()-display_height_)/2;
}

const uint16_t* BoxEmu::palette() const {
  return palette_;
}

bool BoxEmu::video_task_callback(std::mutex &m, std::condition_variable& cv, bool &task_notified) {
  const void *_frame_ptr;
  if (xQueueReceive(video_queue_, &_frame_ptr, portMAX_DELAY) != pdTRUE) {
    return false;
  }
  // Account the blit (everything after the dequeue, whichever path returns)
  // for the per-frame display-time report.
  struct BlitTimer {
    BoxEmu &box;
    uint32_t t0;
    ~BlitTimer() {
      box.blit_cycles_.fetch_add(esp_cpu_get_cycle_count() - t0, std::memory_order_relaxed);
      box.frames_blitted_.fetch_add(1, std::memory_order_relaxed);
    }
  } blit_timer{*this, esp_cpu_get_cycle_count()};
  static constexpr int num_lines_to_write = num_rows_in_framebuffer;
  static auto &box = Bsp::get();
  static const uint16_t *vram0 = (uint16_t*)box.vram0();
  static const uint16_t *vram1 = (uint16_t*)box.vram1();
  static uint16_t vram_index = 0; // has to be static so that it persists between calls
  const int _x_offset = x_offset();
  const int _y_offset = y_offset();
  const uint16_t* _palette = palette();
  // special case: if _frame_ptr is null, then we ignore all palette, scaling,
  // etc. and simply fill the screen with 0
  if (_frame_ptr == nullptr) {
    for (int y=0; y<lcd_height(); y+= num_lines_to_write) {
      Pixel* _buf = (Pixel*)((uint32_t)vram0 * (vram_index ^ 0x01) + (uint32_t)vram1 * vram_index);
      vram_index = vram_index ^ 0x01;
      int num_lines = std::min<int>(num_lines_to_write, lcd_height()-y);
      // memset the buffer to 0
      memset(_buf, 0, lcd_width() * num_lines * sizeof(Pixel));
      box.write_lcd_frame(0, y + _y_offset, lcd_width(), num_lines, reinterpret_cast<uint8_t*>(&_buf[0]));
    }

    // now return
    return false;
  }

  if (is_native()) {
    if (has_palette()) {
      const bool palette_is_power_of_two = palette_size_ && ((palette_size_ & (palette_size_ - 1)) == 0);
      const size_t palette_mask = palette_size_ - 1;
      for (int y=0; y<display_height_; y+= num_lines_to_write) {
        uint16_t* _buf = (uint16_t*)((uint32_t)vram0 * (vram_index ^ 0x01) + (uint32_t)vram1 * vram_index);
        vram_index = vram_index ^ 0x01;
        int num_lines = std::min<int>(num_lines_to_write, display_height_-y);
        const uint8_t* _frame = (const uint8_t*)_frame_ptr;
        for (int i=0; i<num_lines; i++) {
          const uint8_t* src = _frame + (y + i) * native_pitch_;
          uint16_t* dst = _buf + i * display_width_;
          if (palette_is_power_of_two) {
            int j = 0;
            for (; j + 7 < display_width_; j += 8) {
              dst[j + 0] = _palette[src[j + 0] & palette_mask];
              dst[j + 1] = _palette[src[j + 1] & palette_mask];
              dst[j + 2] = _palette[src[j + 2] & palette_mask];
              dst[j + 3] = _palette[src[j + 3] & palette_mask];
              dst[j + 4] = _palette[src[j + 4] & palette_mask];
              dst[j + 5] = _palette[src[j + 5] & palette_mask];
              dst[j + 6] = _palette[src[j + 6] & palette_mask];
              dst[j + 7] = _palette[src[j + 7] & palette_mask];
            }
            for (; j < display_width_; j++) {
              dst[j] = _palette[src[j] & palette_mask];
            }
          } else {
            for (int j = 0; j < display_width_; j++) {
              dst[j] = _palette[src[j] % palette_size_];
            }
          }
        }
        box.write_lcd_frame(_x_offset, y + _y_offset, display_width_, num_lines, reinterpret_cast<uint8_t*>(&_buf[0]));
      }
    } else {
      // no palette
      for (int y=0; y<display_height_; y+= num_lines_to_write) {
        uint16_t* _buf = (uint16_t*)((uint32_t)vram0 * (vram_index ^ 0x01) + (uint32_t)vram1 * vram_index);
        vram_index = vram_index ^ 0x01;
        int num_lines = std::min<int>(num_lines_to_write, display_height_-y);
        const uint16_t* _frame = (const uint16_t*)_frame_ptr;
        for (int i=0; i<num_lines; i++) {
          const uint16_t* src = _frame + (y + i) * native_pitch_;
          uint16_t* dst = _buf + i * display_width_;
          std::memcpy(dst, src, display_width_ * sizeof(uint16_t));
        }
        box.write_lcd_frame(_x_offset, y + _y_offset, display_width_, num_lines, reinterpret_cast<uint8_t*>(&_buf[0]));
      }
    }
  } else {
    // we are scaling the screen (and possibly using a custom palette)
    // if we don't have a custom palette, we just need to scale/fill the frame
    [[maybe_unused]] float y_scale = (float)display_height_/native_height_;
    float x_scale = (float)display_width_/native_width_;
    float inv_x_scale = (float)native_width_/display_width_;
    float inv_y_scale = (float)native_height_/display_height_;
    int max_y = Bsp::lcd_height();
    int max_x = std::clamp<int>(x_scale * native_width_, 0, Bsp::lcd_width());
    if (has_palette()) {
      for (int y=0; y<max_y; y+=num_lines_to_write) {
        // each iteration of the loop, we swap the vram index so that we can
        // write to the other buffer while the other one is being transmitted
        int i = 0;
        uint16_t* _buf = (uint16_t*)((uint32_t)vram0 * (vram_index ^ 0x01) + (uint32_t)vram1 * vram_index);
        vram_index = vram_index ^ 0x01;
        for (; i<num_lines_to_write; i++) {
          int _y = y+i;
          if (_y >= max_y) {
            break;
          }
          int source_y = (float)_y * inv_y_scale;
          const uint8_t* _frame = (const uint8_t*)_frame_ptr;
          // write two pixels (32 bits) at a time because it's faster
          for (int x=0; x<max_x/2; x++) {
            int source_x = (float)x * 2 * inv_x_scale;
            int src_index = source_y*native_pitch_ + source_x;
            int dst_index = i*max_x + x * 2;
            _buf[dst_index] = _palette[_frame[src_index] % palette_size_];
            _buf[dst_index + 1] = _palette[_frame[src_index + 1] % palette_size_];
          }
        }
        box.write_lcd_frame(0 + _x_offset, y, max_x, i, reinterpret_cast<uint8_t*>(&_buf[0]));
      }
    } else {
      // no palette
      for (int y=0; y<max_y; y+=num_lines_to_write) {
        // each iteration of the loop, we swap the vram index so that we can
        // write to the other buffer while the other one is being transmitted
        int i = 0;
        uint16_t* _buf = (uint16_t*)((uint32_t)vram0 * (vram_index ^ 0x01) + (uint32_t)vram1 * vram_index);
        vram_index = vram_index ^ 0x01;
        for (; i<num_lines_to_write; i++) {
          int _y = y+i;
          if (_y >= max_y) {
            break;
          }
          int source_y = (float)_y * inv_y_scale;
          const uint16_t* _frame = (const uint16_t*)_frame_ptr;
          // write two pixels (32 bits) at a time because it's faster
          for (int x=0; x<max_x/2; x++) {
            int source_x = (float)x * 2 * inv_x_scale;
            int src_index = source_y*native_pitch_ + source_x;
            int dst_index = i*max_x + x * 2;
            _buf[dst_index] = _frame[src_index];
            _buf[dst_index + 1] = _frame[src_index + 1];
          }
        }
        box.write_lcd_frame(0 + _x_offset, y, max_x, i, reinterpret_cast<uint8_t*>(&_buf[0]));
      }
    }
  }
  return false;
}
