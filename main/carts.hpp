#pragma once

#include <memory>

#include "core_overlay.h"
#include "display.hpp"

#include "darkforces_cart.hpp"
#include "doom_cart.hpp"
#include "gbc_cart.hpp"
#include "genesis_cart.hpp"
#include "msx_cart.hpp"
#include "nes_cart.hpp"
#include "pico8_cart.hpp"
#include "sms_cart.hpp"

// The code overlay member (components/core_overlay) of a platform's core.
inline const char *overlay_core(Emulator platform) {
  switch (platform) {
  case Emulator::GAMEBOY:
  case Emulator::GAMEBOY_COLOR:
    return "gbc";
  case Emulator::NES:
    return "nes";
  case Emulator::SEGA_MASTER_SYSTEM:
  case Emulator::SEGA_GAME_GEAR:
    return "sms";
  case Emulator::SEGA_GENESIS:
  case Emulator::SEGA_MEGA_DRIVE:
    return "genesis";
  case Emulator::MSX:
    return "msx";
  case Emulator::DOOM:
    return "doom";
  case Emulator::DARK_FORCES:
    return "darkforces";
  case Emulator::PICO8:
    return "pico8";
  default:
    return nullptr;
  }
}

std::unique_ptr<Cart> make_cart(const RomInfo& info, std::shared_ptr<espp::Display<lv_color16_t>> display) {
  // the core's code first (only one core's code is in PSRAM at a time; the
  // caller unloads it when the cart is gone)
  if (const char *core = overlay_core(info.platform); !core || !core_overlay_load(core)) {
    return nullptr;
  }
  switch (info.platform) {
  case Emulator::GAMEBOY:
  case Emulator::GAMEBOY_COLOR:
    return std::make_unique<GbcCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
    break;
  case Emulator::NES:
    return std::make_unique<NesCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  case Emulator::SEGA_MASTER_SYSTEM:
  case Emulator::SEGA_GAME_GEAR:
    return std::make_unique<SmsCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  case Emulator::SEGA_GENESIS:
  case Emulator::SEGA_MEGA_DRIVE:
    return std::make_unique<GenesisCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  case Emulator::MSX:
    return std::make_unique<MsxCart>(Cart::Config{
        .info = info,
        .copy_romdata = false,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  case Emulator::DOOM:
    return std::make_unique<DoomCart>(Cart::Config{
        .info = info,
        .copy_romdata = false,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  case Emulator::DARK_FORCES:
    return std::make_unique<DarkForcesCart>(Cart::Config{
        .info = info,
        .copy_romdata = false,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  case Emulator::PICO8:
    return std::make_unique<Pico8Cart>(Cart::Config{
        .info = info,
        .copy_romdata = false,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  default:
    return nullptr;
  }
}
