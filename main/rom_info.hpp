#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "format.hpp"
#include "string_utils.hpp"

enum class Emulator { UNKNOWN, NES, GAMEBOY, GAMEBOY_COLOR, SEGA_MASTER_SYSTEM, SEGA_GAME_GEAR, GENESIS, SNES };

struct RomInfo {
  std::string name;
  std::string boxart_path;
  std::string rom_path;
  Emulator platform;
};

std::vector<RomInfo> parse_metadata(const std::string& metadata_path);
