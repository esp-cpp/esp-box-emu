#include "usb.hpp"

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,

    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "Finger563",                      // 1: Manufacturer
    "ESP-Box-Emu",                  // 2: Product
    "123456",                       // 3: Serials
    "Box-Emu uSD Card",                     // 4. MSC
};

// callback that is delivered when storage is mounted/unmounted by application.
static void storage_mount_changed_cb(tinyusb_msc_event_t *event)
{
    fmt::print("Storage mounted to application: {}\n", event->mount_changed_data.is_mounted ? "Yes" : "No");
}

static bool usb_enabled_ = false;

bool usb_is_enabled() {
  return usb_enabled_;
}

void usb_init() {
  // get the card from the filesystem initialization
  auto card = get_sdcard();
  if (!card) {
    fmt::print("No SD card found, skipping USB MSC initialization\n");
    return;
  }

  fmt::print("USB MSC initialization\n");
  // register the callback for the storage mount changed event.
  const tinyusb_msc_sdmmc_config_t config_sdmmc = {
    .card = card,
    .callback_mount_changed = storage_mount_changed_cb,
    .mount_config = {
      .max_files = 5,
    }
  };
  ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&config_sdmmc));
  ESP_ERROR_CHECK(tinyusb_msc_register_callback(TINYUSB_MSC_EVENT_MOUNT_CHANGED, storage_mount_changed_cb));

  // initialize the tinyusb stack
  fmt::print("USB MSC initialization\n");
  const tinyusb_config_t tusb_cfg = {
    .device_descriptor = &descriptor_config,
    .string_descriptor = string_desc_arr,
    .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
    .external_phy = false,
    .configuration_descriptor = desc_configuration,
  };
  ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
  fmt::print("USB MSC initialization DONE\n");
  usb_enabled_ = true;
}

void usb_deinit() {
  if (!usb_enabled_) {
    return;
  }
  fmt::print("USB MSC deinitialization\n");
  auto err = tinyusb_driver_uninstall();
  if (err != ESP_OK) {
    fmt::print("tinyusb_driver_uninstall failed: {}\n", esp_err_to_name(err));
  }
  usb_enabled_ = false;
  // and reconnect the CDC port, see:
  // https://github.com/espressif/idf-extra-components/pull/229
  usb_phy_config_t phy_conf = {
    .controller = (usb_phy_controller_t)1, // NOTE: for some reason, USB_PHY_CTRL_SERIAL_JTAG is not defined in the SDK for the ESP32s3
  };
  usb_phy_handle_t jtag_phy;
  usb_new_phy(&phy_conf, &jtag_phy);
}
