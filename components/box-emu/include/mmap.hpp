#pragma once

#include <string>

#include <esp_err.h>
#include "nvs_flash.h"
#include "spi_flash_mmap.h"
#include "esp_partition.h"

#include "format.hpp"

void init_memory();
size_t copy_romdata_to_cart_partition(const std::string& rom_filename);
uint8_t *get_mmapped_romdata();
