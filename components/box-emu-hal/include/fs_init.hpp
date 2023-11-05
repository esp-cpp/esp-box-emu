#pragma once

#include <filesystem>

#include <esp_err.h>
#include <sys/stat.h>
#include <errno.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#define MOUNT_POINT "/sdcard"

void fs_init();
