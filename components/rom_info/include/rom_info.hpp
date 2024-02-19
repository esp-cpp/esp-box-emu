#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "fs_init.hpp"
#include "format.hpp"
#include "string_utils.hpp"

enum class Emulator { UNKNOWN, NES, GAMEBOY, GAMEBOY_COLOR, SEGA_MASTER_SYSTEM, SEGA_GAME_GEAR, SEGA_GENESIS, SEGA_MEGA_DRIVE, SNES, MSX, DOOM };

struct RomInfo {
  std::string name;
  std::string boxart_path;
  std::string rom_path;
  Emulator platform;
};

// operator == for RomInfo
[[maybe_unused]] static bool operator==(const RomInfo& lhs, const RomInfo& rhs) {
  return
    lhs.name == rhs.name &&
    lhs.boxart_path == rhs.boxart_path &&
    lhs.rom_path == rhs.rom_path &&
    lhs.platform == rhs.platform;
}

std::vector<RomInfo> parse_metadata(const std::string& metadata_path);

// for easy printing of Emulator using libfmt
template <>
struct fmt::formatter<Emulator> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const Emulator& platform, FormatContext& ctx) {
    switch (platform) {
    case Emulator::UNKNOWN:
      return fmt::format_to(ctx.out(), "UNKNOWN");
    case Emulator::NES:
      return fmt::format_to(ctx.out(), "NES");
    case Emulator::GAMEBOY:
      return fmt::format_to(ctx.out(), "GAMEBOY");
    case Emulator::GAMEBOY_COLOR:
      return fmt::format_to(ctx.out(), "GAMEBOY_COLOR");
    case Emulator::SEGA_MASTER_SYSTEM:
      return fmt::format_to(ctx.out(), "SEGA_MASTER_SYSTEM");
    case Emulator::SEGA_GAME_GEAR:
      return fmt::format_to(ctx.out(), "SEGA_GAME_GEAR");
    case Emulator::SEGA_GENESIS:
      return fmt::format_to(ctx.out(), "GENESIS");
    case Emulator::SEGA_MEGA_DRIVE:
      return fmt::format_to(ctx.out(), "MEGA_DRIVE");
    case Emulator::SNES:
      return fmt::format_to(ctx.out(), "SNES");
    case Emulator::MSX:
      return fmt::format_to(ctx.out(), "MSX");
    case Emulator::DOOM:
      return fmt::format_to(ctx.out(), "DOOM");
    default:
      return fmt::format_to(ctx.out(), "UNKNOWN");
    }
  }
};

// for easy printing of RomInfo using libfmt
template <>
struct fmt::formatter<RomInfo> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const RomInfo& info, FormatContext& ctx) {
    return fmt::format_to(ctx.out(), "RomInfo {{ name: {}, boxart_path: {}, rom_path: {}, platform: {} }}\n",
                          info.name, info.boxart_path, info.rom_path, info.platform);
  }
};
