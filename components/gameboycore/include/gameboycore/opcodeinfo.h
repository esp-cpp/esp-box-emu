/**
 * \file opcodeinfo.h
 * \brief Define opcode metadata
 * \author Natesh Narain <nnaraindev@gmail.com>
*/
#ifndef GAMEBOYCORE_OPCODEINFO_H
#define GAMEBOYCORE_OPCODEINFO_H

#include <stdint.h>

namespace gb
{
    /**
        Which page of instructions
    */
    enum class OpcodePage
    {
        PAGE1, PAGE2
    };

    enum class OperandType
    {
        NONE, // No operands
        IMM8, // 8 bit immediate data
        IMM16 // 16 bit immediate data
    };

    /**
        Struct containing metadata
    */
    struct OpcodeInfo
    {
        uint8_t cycles;
        const char *disassembly;
        OperandType userdata;

        OpcodeInfo(uint8_t cycles, const char* disassembly, OperandType userdata = OperandType::NONE)
            : cycles{cycles}
            , disassembly{disassembly}
            , userdata{userdata}
        {
        }
    };

    OpcodeInfo getOpcodeInfo(uint8_t opcode, OpcodePage page);
}

#endif // GAMEBOY_OPCODEINFO_H
