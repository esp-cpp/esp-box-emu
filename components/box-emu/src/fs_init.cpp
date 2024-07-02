#include "fs_init.hpp"

static sdmmc_card_t *sdcard = nullptr;

static void sdcard_init() {
  esp_err_t ret;

  // Options for mounting the filesystem. If format_if_mount_failed is set to
  // true, SD card will be partitioned and formatted in case when mounting
  // fails.
  esp_vfs_fat_sdmmc_mount_config_t mount_config;
  memset(&mount_config, 0, sizeof(mount_config));
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 5;
  mount_config.allocation_unit_size = 16 * 1024;
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
  host.slot = sdcard_spi_num;
  // host.max_freq_khz = 20 * 1000;

  spi_bus_config_t bus_cfg;
  memset(&bus_cfg, 0, sizeof(bus_cfg));
  bus_cfg.mosi_io_num = sdcard_mosi;
  bus_cfg.miso_io_num = sdcard_miso;
  bus_cfg.sclk_io_num = sdcard_sclk;
  bus_cfg.quadwp_io_num = -1;
  bus_cfg.quadhd_io_num = -1;
  bus_cfg.max_transfer_sz = 8192;
  spi_host_device_t host_id = (spi_host_device_t)host.slot;
  ret = spi_bus_initialize(host_id, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    fmt::print("Failed to initialize bus.\n");
    return;
  }

  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = sdcard_cs;
  slot_config.host_id = host_id;

  fmt::print("Mounting filesystem\n");
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &sdcard);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      fmt::print("Failed to mount filesystem. "
                 "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.\n");
    } else {
      fmt::print("Failed to initialize the card ({}). "
                 "Make sure SD card lines have pull-up resistors in place.\n", esp_err_to_name(ret));
    }
    return;
  }
  fmt::print("Filesystem mounted\n");

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, sdcard);
}

void fs_init() {
  if (sdcard) return;
  sdcard_init();
}

sdmmc_card_t *get_sdcard() {
  return sdcard;
}
