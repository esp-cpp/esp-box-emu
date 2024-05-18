#include "rom_info.hpp"

std::vector<RomInfo> parse_metadata(const std::string& metadata_path) {
  const std::string fs_prefix = std::string(MOUNT_POINT) + "/";
  std::vector<RomInfo> infos;
  // load metadata path
  std::ifstream metadata(fs_prefix + metadata_path, std::ios::in);
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
    Emulator platform = Emulator::UNKNOWN;
    if (endsWith(rom_path, ".nes")) { // nes
      platform = Emulator::NES;
    } else if (endsWith(rom_path, ".gb")) { // gameboy
      platform = Emulator::GAMEBOY;
    } else if (endsWith(rom_path, ".gbc")) { // gameboy color
      platform = Emulator::GAMEBOY_COLOR;
    } else if (endsWith(rom_path, ".sms")) { // sega master system
      platform = Emulator::SEGA_MASTER_SYSTEM;
    } else if (endsWith(rom_path, ".gg")) { // sega game gear
      platform = Emulator::SEGA_GAME_GEAR;
    } else if (endsWith(rom_path, ".gen")) { // sega genesis
      platform = Emulator::SEGA_GENESIS;
    } else if (endsWith(rom_path, ".md")) { // sega mega drive
      platform = Emulator::SEGA_MEGA_DRIVE;
    } else if (endsWith(rom_path, ".sfc")) { // snes
      platform = Emulator::SNES;
    } else if (endsWith(rom_path, ".rom")) { // msx
      platform = Emulator::MSX;
    } else if (endsWith(rom_path, ".wad")) { // doom
      platform = Emulator::DOOM;
    }
    if (platform != Emulator::UNKNOWN) {
      // for each row, create rom entry
      infos.emplace_back(name,
                         fs_prefix + boxart_path,
                         fs_prefix +rom_path,
                         platform);
      fmt::print("{}", infos.back());
    }
  }

  return infos;
}
