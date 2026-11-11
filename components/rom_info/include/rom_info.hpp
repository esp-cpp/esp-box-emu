#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "box-emu.hpp"
#include "format.hpp"
#include "string_utils.hpp"

enum class Emulator { UNKNOWN, NES, GAMEBOY, GAMEBOY_COLOR, SEGA_MASTER_SYSTEM, SEGA_GAME_GEAR, SEGA_GENESIS, SEGA_MEGA_DRIVE, SNES, MSX, DOOM, DARK_FORCES, PICO8 };

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

/// The console a game is played as, in the order the game list shows them
/// (several emulators can share one, e.g. Genesis / Mega Drive; the PC ports
/// count as PC).
enum class Console : uint8_t { NES, SNES, GAME_BOY, GAME_BOY_COLOR, MASTER_SYSTEM, GAME_GEAR, GENESIS, MSX, PC, PICO8, UNKNOWN };

/// The shape the firmware draws for a console that has no art on the SD card.
enum class ConsoleShape : uint8_t { GAMEPAD, HANDHELD_TALL, HANDHELD_WIDE, DISC, COMPUTER, FANTASY };

struct ConsoleInfo {
  const char *name;       ///< shown in the list header
  const char *short_name; ///< shown in the badge
  const char *key;        ///< art on the SD card: boxart/consoles/<key>.jpg
  uint32_t color;         ///< badge accent (0xRRGGBB)
  ConsoleShape shape;
};

/// The console `platform` games are grouped under.
Console console_of(Emulator platform);
/// Name, art key and colors of `console`.
const ConsoleInfo &console_info(Console console);

// for easy printing of Emulator using libfmt
template <>
struct fmt::formatter<Emulator> {
  constexpr auto parse(format_parse_context& ctx) const { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const Emulator& platform, FormatContext& ctx) const {
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
    case Emulator::DARK_FORCES:
      return fmt::format_to(ctx.out(), "DARK_FORCES");
    case Emulator::PICO8:
      return fmt::format_to(ctx.out(), "PICO8");
    default:
      return fmt::format_to(ctx.out(), "UNKNOWN");
    }
  }
};

// for easy printing of RomInfo using libfmt
template <>
struct fmt::formatter<RomInfo> {
  constexpr auto parse(format_parse_context& ctx) const { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const RomInfo& info, FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "RomInfo {{ name: {}, boxart_path: {}, rom_path: {}, platform: {} }}\n",
                          info.name, info.boxart_path, info.rom_path, info.platform);
  }
};
