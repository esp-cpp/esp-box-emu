#include "rom_info.hpp"

std::vector<RomInfo> parse_metadata(const std::string& metadata_path) {
  const std::string fs_prefix = std::string(BoxEmu::mount_point) + "/";
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
#ifdef ENABLE_NES
      platform = Emulator::NES;
#endif
    } else if (endsWith(rom_path, ".gb")) { // gameboy
#ifdef ENABLE_GBC
      platform = Emulator::GAMEBOY;
#endif
    } else if (endsWith(rom_path, ".gbc")) { // gameboy color
#ifdef ENABLE_GBC
      platform = Emulator::GAMEBOY_COLOR;
#endif
    } else if (endsWith(rom_path, ".sms")) { // sega master system
#ifdef ENABLE_SMS
      platform = Emulator::SEGA_MASTER_SYSTEM;
#endif
    } else if (endsWith(rom_path, ".gg")) { // sega game gear
#ifdef ENABLE_SMS
      platform = Emulator::SEGA_GAME_GEAR;
#endif
    } else if (endsWith(rom_path, ".gen")) { // sega genesis
#ifdef ENABLE_GENESIS
      platform = Emulator::SEGA_GENESIS;
#endif
    } else if (endsWith(rom_path, ".md")) { // sega mega drive
#ifdef ENABLE_GENESIS
      platform = Emulator::SEGA_MEGA_DRIVE;
#endif
    } else if (endsWith(rom_path, ".sfc")) { // snes
#ifdef ENABLE_SNES
      platform = Emulator::SNES;
#endif
    } else if (endsWith(rom_path, ".rom")) { // msx
#ifdef ENABLE_MSX
      platform = Emulator::MSX;
#endif
    } else if (endsWith(rom_path, ".wad")) { // doom
#ifdef ENABLE_DOOM
      platform = Emulator::DOOM;
#endif
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
  fmt::print("Loaded {} roms from metadata file {}\n", infos.size(), metadata_path);

  return infos;
}
