#pragma once

#include <memory>

#include "display.hpp"

#include "gbc_cart.hpp"
#include "nes_cart.hpp"
#include "genesis_cart.hpp"
#include "sms_cart.hpp"

std::unique_ptr<Cart> make_cart(const RomInfo& info, std::shared_ptr<espp::Display> display) {
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
  default:
    return nullptr;
  }
}
