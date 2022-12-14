#pragma once

#include <filesystem>

#include <esp_err.h>
#include <sys/stat.h>
#include <errno.h>

#if CONFIG_ROM_STORAGE_LITTLEFS
#include "esp_littlefs.h"
#define MOUNT_POINT "/littlefs"
#elif CONFIG_ROM_STORAGE_SDCARD
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#define MOUNT_POINT "/sdcard"
#endif

#define DEFAULT_MODE      S_IRWXU | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH

bool mkdirp(const char* path, mode_t mode = DEFAULT_MODE);
void fs_init();
