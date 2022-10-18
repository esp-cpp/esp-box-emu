#include "fs_init.hpp"

#include "format.hpp"

static esp_vfs_littlefs_conf_t conf = {
  .base_path = "/littlefs",
  .partition_label = "littlefs",
  .format_if_mount_failed = true,
  .dont_mount = false,
};

void fs_init() {
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
