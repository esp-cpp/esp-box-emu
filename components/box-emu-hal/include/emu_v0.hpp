#pragma once

#if CONFIG_HARDWARE_V0
#include "mcp23x17.hpp"
using InputDriver = espp::Mcp23x17;
static constexpr uint16_t START_PIN =  (1<<0) << 0; // start pin is on port a of the MCP23x17
static constexpr uint16_t SELECT_PIN = (1<<1) << 0; // select pin is on port a of the MCP23x17
static constexpr uint16_t UP_PIN =    (1<<0) << 8; // up pin is on port b of the MCP23x17
static constexpr uint16_t DOWN_PIN =  (1<<1) << 8; // down pin is on port b of the MCP23x17
static constexpr uint16_t LEFT_PIN =  (1<<2) << 8; // left pin is on port b of the MCP23x17
static constexpr uint16_t RIGHT_PIN = (1<<3) << 8; // right pin is on port b of the MCP23x17
static constexpr uint16_t A_PIN =     (1<<4) << 8; // a pin is on port b of the MCP23x17
static constexpr uint16_t B_PIN =     (1<<5) << 8; // b pin is on port b of the MCP23x17
static constexpr uint16_t X_PIN =     (1<<6) << 8; // x pin is on port b of the MCP23x17
static constexpr uint16_t Y_PIN =     (1<<7) << 8; // y pin is on port b of the MCP23x17
static constexpr uint16_t DIRECTION_MASK = (UP_PIN | DOWN_PIN | LEFT_PIN | RIGHT_PIN | A_PIN | B_PIN | X_PIN | Y_PIN | START_PIN | SELECT_PIN);
static constexpr uint16_t INTERRUPT_MASK = (START_PIN | SELECT_PIN);
static constexpr uint8_t PORT_0_DIRECTION_MASK = DIRECTION_MASK & 0xFF;
static constexpr uint8_t PORT_1_DIRECTION_MASK = (DIRECTION_MASK >> 8) & 0xFF;
static constexpr uint8_t PORT_0_INTERRUPT_MASK = INTERRUPT_MASK & 0xFF;
static constexpr uint8_t PORT_1_INTERRUPT_MASK = (INTERRUPT_MASK >> 8) & 0xFF;
#endif
