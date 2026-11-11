// Bootloader hook: boot the updater app (ota_1) once, when the app asked for
// it through an RTC register.
//
// The box's app cannot write its own flash: with the flash and the octal PSRAM
// at 120 MHz its writes report success but do not land (they do at 80 MHz,
// which Sonic 3 cannot afford). The bootloader runs the flash at the image
// header's speed, so it writes the otadata entry instead, before the usual
// boot partition selection reads it.
#include <string.h>

#include "bootloader_common.h"
#include "bootloader_flash_priv.h"
#include "esp_flash_partitions.h"
#include "esp_log.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// Shared with main/remote_control.hpp (BootSwitch). RTC_CNTL_STORE0 is
// reserved for the application; it survives a software reset.
#define UPDATER_SWITCH_REG RTC_CNTL_STORE0_REG
#define UPDATER_SWITCH_MAGIC 0x55504454u // "UPDT"

static const char *TAG = "updater_switch";

// keep this file in the bootloader link (its hooks are weak symbols)
void bootloader_hooks_include(void) {}

static bool find_partitions(esp_partition_pos_t *otadata, int *app_slots) {
  esp_partition_info_t entry;
  bool found = false;
  *app_slots = 0;
  for (int i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES; i++) {
    if (bootloader_flash_read(ESP_PARTITION_TABLE_OFFSET + i * sizeof(entry), &entry, sizeof(entry), false) != ESP_OK ||
        entry.magic != ESP_PARTITION_MAGIC) {
      break;
    }
    if (entry.type == PART_TYPE_DATA && entry.subtype == PART_SUBTYPE_DATA_OTA) {
      *otadata = entry.pos;
      found = true;
    } else if (entry.type == PART_TYPE_APP && (entry.subtype & ~PART_SUBTYPE_OTA_MASK) == PART_SUBTYPE_OTA_FLAG) {
      (*app_slots)++;
    }
  }
  return found && *app_slots >= 2;
}

void bootloader_after_init(void) {
  if (REG_READ(UPDATER_SWITCH_REG) != UPDATER_SWITCH_MAGIC) {
    return;
  }
  REG_WRITE(UPDATER_SWITCH_REG, 0); // once: a failed updater boots the app again
  esp_partition_pos_t otadata;
  int slots;
  if (!find_partitions(&otadata, &slots)) {
    ESP_LOGE(TAG, "no otadata / ota_1 partition");
    return;
  }
  const uint32_t sector = 0x1000;
  esp_ota_select_entry_t e[2];
  for (int i = 0; i < 2; i++) {
    if (bootloader_flash_read(otadata.offset + i * sector, &e[i], sizeof(e[i]), false) != ESP_OK) {
      return;
    }
  }
  const bool v0 = bootloader_common_ota_select_valid(&e[0]), v1 = bootloader_common_ota_select_valid(&e[1]);
  uint32_t seq = 0;
  if (v0 && e[0].ota_seq > seq) seq = e[0].ota_seq;
  if (v1 && e[1].ota_seq > seq) seq = e[1].ota_seq;
  uint32_t next = seq + 1;
  while ((next - 1) % slots != 1) next++; // slot 1: the updater
  // overwrite the older (or an invalid) entry, so the current one survives a failure
  const int target = !v0 ? 0 : !v1 ? 1 : (e[0].ota_seq <= e[1].ota_seq ? 0 : 1);
  esp_ota_select_entry_t entry;
  memset(&entry, 0xFF, sizeof(entry));
  entry.ota_seq = next;
  entry.ota_state = ESP_OTA_IMG_VALID;
  entry.crc = bootloader_common_ota_select_crc(&entry);
  const size_t addr = otadata.offset + target * sector;
  if (bootloader_flash_erase_sector(addr / sector) != ESP_OK ||
      bootloader_flash_write(addr, &entry, sizeof(entry), false) != ESP_OK) {
    ESP_LOGE(TAG, "could not write otadata");
    return;
  }
  ESP_LOGI(TAG, "booting the updater (otadata seq %u)", (unsigned)next);
}
