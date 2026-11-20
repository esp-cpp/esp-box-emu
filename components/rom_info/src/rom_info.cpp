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
    // (split at the first two commas only: the rest, commas and all, is the name)
    const auto first = line.find(',');
    if (first == std::string::npos) {
      continue;
    }
    const auto second = line.find(',', first + 1);
    rom_path = trim(line.substr(0, first));
    boxart_path = trim(line.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1));
    if (second != std::string::npos) {
      name = trim(line.substr(second + 1));
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
    } else if (endsWith(rom_path, ".gob") || endsWith(rom_path, ".GOB")) { // dark forces (DARK.GOB)
#ifdef ENABLE_DARKFORCES
      platform = Emulator::DARK_FORCES;
#endif
    } else if (endsWith(rom_path, ".p8") || endsWith(rom_path, ".p8.png") || endsWith(rom_path, ".P8") || endsWith(rom_path, ".P8.PNG")) { // pico-8
#ifdef ENABLE_PICO8
      platform = Emulator::PICO8;
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

Console console_of(Emulator platform) {
  switch (platform) {
  case Emulator::NES:
    return Console::NES;
  case Emulator::SNES:
    return Console::SNES;
  case Emulator::GAMEBOY:
    return Console::GAME_BOY;
  case Emulator::GAMEBOY_COLOR:
    return Console::GAME_BOY_COLOR;
  case Emulator::SEGA_MASTER_SYSTEM:
    return Console::MASTER_SYSTEM;
  case Emulator::SEGA_GAME_GEAR:
    return Console::GAME_GEAR;
  case Emulator::SEGA_GENESIS:
  case Emulator::SEGA_MEGA_DRIVE:
    return Console::GENESIS;
  case Emulator::MSX:
    return Console::MSX;
  case Emulator::DOOM:
  case Emulator::DARK_FORCES:
    return Console::PC;
  case Emulator::PICO8:
    return Console::PICO8;
  default:
    return Console::UNKNOWN;
  }
}

const ConsoleInfo &console_info(Console console) {
  static const ConsoleInfo infos[] = {
      {"NES", "NES", "nes"},
      {"Super Nintendo", "SNES", "snes"},
      {"Game Boy", "Game Boy", "gb"},
      {"Game Boy Color", "GB Color", "gbc"},
      {"Master System", "SMS", "sms"},
      {"Game Gear", "Game Gear", "gg"},
      {"Sega Genesis", "Genesis", "genesis"},
      {"MSX", "MSX", "msx"},
      {"PC", "PC", "pc"},
      {"PICO-8", "PICO-8", "pico8"},
      {"Other", "Other", "other"},
  };
  const auto i = static_cast<size_t>(console);
  return infos[i < std::size(infos) ? i : std::size(infos) - 1];
}
