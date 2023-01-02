#include "fs_init.hpp"

#include "format.hpp"

/** Utility function to create directory tree */
bool mkdirp(const char* path, mode_t mode) {
  // return std::filesystem::create_directories(path);

  // Invalid string
  if(path[0] == '\0') {
    return false;
  }

  // const cast for hack
  char* p = const_cast<char*>(path);

  // Find next slash mkdir() it and until we're at end of string
  while (*p != '\0') {
    // Skip first character
    p++;

    // Find first slash or end
    while(*p != '\0' && *p != '/') p++;

    // Remember value from p
    char v = *p;

    // Write end of string at p
    *p = '\0';

    fmt::print("Making directory: {} ({})\n", path, 0775);
    // Create folder from path to '\0' inserted at p
    auto ret = mkdir(path, mode);
    if(ret != 0 && errno != EEXIST) {
      fmt::print("could not make {}, {} - {}\n", path, ret, errno);
      *p = v;
      return false;
    } else {
      fmt::print("made {}\n", path);
    }

    // Restore path to it's former glory
    *p = v;
  }

  return true;
}

#if CONFIG_ROM_STORAGE_LITTLEFS
static esp_vfs_littlefs_conf_t conf = {
  .base_path = MOUNT_POINT,
  .partition_label = "littlefs",
  .format_if_mount_failed = true,
  .dont_mount = false,
};

static void littlefs_init() {
  // Use settings defined above to initialize and mount LittleFS filesystem.
  // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      fmt::print("Failed to mount or format filesystem\n");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      fmt::print("Failed to find LittleFS partition\n");
    } else {
      fmt::print("Failed to initialize LittleFS ({})\n", esp_err_to_name(ret));
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_littlefs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    fmt::print("Failed to get LittleFS partition information ({})\n",
               esp_err_to_name(ret));
  } else {
    fmt::print("Partition size: total: {}, used: {}\n", total, used);
  }
}
#endif

#if CONFIG_ROM_STORAGE_SDCARD
static void sdcard_init() {
  esp_err_t ret;

  // Options for mounting the filesystem. If format_if_mount_failed is set to
  // true, SD card will be partitioned and formatted in case when mounting
  // fails.
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };
  sdmmc_card_t *card;
  const char mount_point[] = "/sdcard";
  fmt::print("Initializing SD card\n");

  // Use settings defined above to initialize SD card and mount FAT filesystem.
  // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
  // Please check its source code and implement error recovery when developing
  // production applications.
  fmt::print("Using SPI peripheral\n");

  // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
  // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
  // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI3_HOST;
  // host.max_freq_khz = 10 * 1000;

  spi_bus_config_t bus_cfg = {
    .mosi_io_num = GPIO_NUM_11,
    .miso_io_num = GPIO_NUM_13,
    .sclk_io_num = GPIO_NUM_12,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 8192,
  };
  spi_host_device_t host_id = (spi_host_device_t)host.slot;
  ret = spi_bus_initialize(host_id, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    fmt::print("Failed to initialize bus.\n");
    return;
  }

  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = GPIO_NUM_10;
  slot_config.host_id = host_id;

  fmt::print("Mounting filesystem\n");
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      fmt::print("Failed to mount filesystem. "
                 "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.\n");
    } else {
      fmt::print("Failed to initialize the card (%s). "
                 "Make sure SD card lines have pull-up resistors in place.\n", esp_err_to_name(ret));
    }
    return;
  }
  fmt::print("Filesystem mounted\n");

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);
}
#endif

void fs_init() {
#if CONFIG_ROM_STORAGE_LITTLEFS
  littlefs_init();
#elif CONFIG_ROM_STORAGE_SDCARD
  sdcard_init();
#endif
}
