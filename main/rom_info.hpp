#pragma once

#include <fstream>
#include <string>

#include "format.hpp"

#include "string_utils.hpp"

enum class Emulator { UNKNOWN, NES, GAMEBOY, GAMEBOY_COLOR, SEGA_MASTER_SYSTEM, GENESIS, SNES };

struct RomInfo {
  std::string name;
  std::string boxart_path;
  std::string rom_path;
  Emulator platform;
};

std::vector<RomInfo> parse_metadata(const std::string& metadata_path) {
  std::vector<RomInfo> infos;
  // load metadata path
  std::ifstream metadata(metadata_path, std::ios::in);
  if (!metadata.is_open()) {
    fmt::print("Couldn't load metadata file {}!\n", metadata_path);
    return infos;
  }
  // parse it as csv, format = rom_path, boxart_path, name - name is last
  // because it might have commas in it.
  std::string line;
  while (std::getline(metadata, line)) {
    // get the fields from each line
    std::string rom_path, boxart_path, name;
    char *str = line.data();
    char *token = strtok(str, ",");
    int num_tokens = 0;
    while (token != NULL && num_tokens < 3) {
      switch (num_tokens) {
      case 0:
        // rom path
        rom_path = token;
        rom_path = trim(rom_path);
        break;
      case 1:
        // boxart path
        boxart_path = token;
        boxart_path = trim(boxart_path);
        break;
      case 2:
        // name
        name = token;
        name = trim(name);
        break;
      default:
        // DANGER WILL ROBINSON
        break;
      }
      token = strtok(NULL, ",");
      num_tokens++;
    }
    fmt::print("INFO: '{}', '{}', '{}'\n", rom_path, boxart_path, name);
    Emulator platform = Emulator::UNKNOWN;
    if (endsWith(rom_path, ".nes")) {
      platform = Emulator::NES;
    } else if (endsWith(rom_path, ".gb")) {
      platform = Emulator::GAMEBOY;
    } else if (endsWith(rom_path, ".gbc")) {
      platform = Emulator::GAMEBOY_COLOR;
    }
    if (platform != Emulator::UNKNOWN) {
      // for each row, create rom entry
      infos.emplace_back(name, boxart_path, rom_path, platform);
    }
  }

  return infos;
}
