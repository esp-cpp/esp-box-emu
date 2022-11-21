#pragma once

#include <esp_err.h>
#include <sys/stat.h>

#if CONFIG_ROM_STORAGE_LITTLEFS
#include "esp_littlefs.h"
#define MOUNT_POINT "/littlefs"
#elif CONFIG_ROM_STORAGE_SDCARD
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#define MOUNT_POINT "/sdcard"
#endif

void fs_init();
