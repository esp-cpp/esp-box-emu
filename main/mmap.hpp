#pragma once

#include <fstream>
#include <string>

#include <esp_err.h>
#include "nvs_flash.h"
#include "esp_partition.h"

#include "format.hpp"

void init_memory();
void copy_romdata_to_nesgame_partition(const std::string& rom_filename);
uint8_t *get_mmapped_romdata();
