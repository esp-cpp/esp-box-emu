#pragma once

#include <tinyusb.h>
#include <tusb_msc_storage.h>

#include "fs_init.hpp"

bool usb_is_enabled();
void usb_init();
void usb_deinit();
