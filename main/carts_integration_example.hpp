#pragma once

// This file shows the integration points for adding PICO-8 support to the main cart system

// 1. Add to main/carts.hpp includes section:
#if defined(ENABLE_PICO8)
#include "pico8_cart.hpp"
#endif

// 2. Add to the emulator enum in cart.hpp:
enum class Emulator {
  UNKNOWN = 0,
  NES,
  GAMEBOY,
  GAMEBOY_COLOR,
  SMS,
  GENESIS,
  MSX,
  DOOM,
  PICO8,  // <- Add this line
};

// 3. Add to the get_emulator_from_extension function:
static Emulator get_emulator_from_extension(const std::string& extension) {
  if (extension == ".nes") return Emulator::NES;
  if (extension == ".gb") return Emulator::GAMEBOY;
  if (extension == ".gbc") return Emulator::GAMEBOY_COLOR;
  if (extension == ".sms") return Emulator::SMS;
  if (extension == ".gg") return Emulator::SMS;
  if (extension == ".gen") return Emulator::GENESIS;
  if (extension == ".md") return Emulator::GENESIS;
  if (extension == ".rom") return Emulator::MSX;
  if (extension == ".wad") return Emulator::DOOM;
  if (extension == ".p8") return Emulator::PICO8;  // <- Add this line
  return Emulator::UNKNOWN;
}

// 4. Add to the get_emulator_name function:
static std::string get_emulator_name(Emulator emulator) {
  switch (emulator) {
    case Emulator::NES: return "NES";
    case Emulator::GAMEBOY: return "Game Boy";
    case Emulator::GAMEBOY_COLOR: return "Game Boy Color";
    case Emulator::SMS: return "SMS/Game Gear";
    case Emulator::GENESIS: return "Genesis/Mega Drive";
    case Emulator::MSX: return "MSX";
    case Emulator::DOOM: return "Doom";
    case Emulator::PICO8: return "PICO-8";  // <- Add this line
    default: return "Unknown";
  }
}

// 5. Add to the cart creation factory function:
static std::unique_ptr<Cart> make_cart(const Cart::Config& config) {
  switch (config.info.platform) {
#if defined(ENABLE_NES)
    case Emulator::NES:
      return std::make_unique<NesCart>(config);
#endif
#if defined(ENABLE_GBC)
    case Emulator::GAMEBOY:
    case Emulator::GAMEBOY_COLOR:
      return std::make_unique<GbcCart>(config);
#endif
#if defined(ENABLE_SMS)
    case Emulator::SMS:
      return std::make_unique<SmsCart>(config);
#endif
#if defined(ENABLE_GENESIS)
    case Emulator::GENESIS:
      return std::make_unique<GenesisCart>(config);
#endif
#if defined(ENABLE_MSX)
    case Emulator::MSX:
      return std::make_unique<MsxCart>(config);
#endif
#if defined(ENABLE_DOOM)
    case Emulator::DOOM:
      return std::make_unique<DoomCart>(config);
#endif
#if defined(ENABLE_PICO8)
    case Emulator::PICO8:  // <- Add this case
      return std::make_unique<Pico8Cart>(config);
#endif
    default:
      return nullptr;
  }
}

// 6. Add to main/CMakeLists.txt in the SRCS list:
/*
set(SRCS
  main.cpp
  cart.hpp
  carts.hpp
  doom_cart.hpp
  gbc_cart.hpp
  genesis_cart.hpp
  heap_utils.hpp
  msx_cart.hpp
  nes_cart.hpp
  pico8_cart.hpp  # <- Add this line
  sms_cart.hpp
)
*/

// 7. Add conditional compilation in main/CMakeLists.txt:
/*
if(CONFIG_ENABLE_PICO8)
  list(APPEND COMPONENT_REQUIRES pico8)
endif()
*/