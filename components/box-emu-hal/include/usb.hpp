#pragma once

#include <sdkconfig.h>

#include <esp_err.h>
#include <hal/usb_phy_types.h>
#include <esp_private/usb_phy.h>

#include <tinyusb.h>
#include <tusb_msc_storage.h>

#include "fs_init.hpp"

bool usb_is_enabled();
void usb_init();
void usb_deinit();
