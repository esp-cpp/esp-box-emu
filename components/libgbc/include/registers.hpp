#pragma once
#include "common.hpp"
#include <string>
#include <stdexcept>

namespace gbc
{
enum flags_t
{
    MASK_ZERO = 0x80,
    MASK_NEGATIVE = 0x40,
    MASK_HALFCARRY = 0x20,
    MASK_CARRY = 0x10,
};

struct regs_t
{
    union
    {
        struct
        {
            uint8_t flags;
            uint8_t accum;
        };
        uint16_t af;
    };
    union
    {
        struct
        {
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc;
    };
    union
    {
        struct
        {
            uint8_t e;
            uint8_t d;
        };
        uint16_t de;
    };
    union
    {
        struct
        {
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl;
    };

    uint16_t sp;
    uint16_t pc;

    inline uint16_t& getreg(const uint8_t bf, const bool use_sp)
    {
        switch (bf & 0x3)
        {
        case 0:
            return bc;
        case 1:
            return de;
        case 2:
            return hl;
        case 3:
            return (use_sp) ? sp : af;
        }
        __builtin_unreachable();
    }
    uint16_t& getreg_sp(const uint8_t opcode) { return getreg((opcode >> 4) & 0x3, true); }
    uint16_t& getreg_af(const uint8_t opcode) { return getreg((opcode >> 4) & 0x3, false); }

    uint8_t& getdest(const uint8_t bf)
    {
        switch (bf & 0x7)
        {
        case 0:
            return b;
        case 1:
            return c;
        case 2:
            return d;
        case 3:
            return e;
        case 4:
            return h;
        case 5:
            return l;
        case 6:
            throw MachineException("getdest: (HL) not accessible here");
        case 7:
            return accum;
        }
        __builtin_unreachable();
    }

    bool compare_flags(const uint8_t opcode) noexcept
    {
        const uint8_t idx = (opcode >> 3) & 0x3;
        if (idx == 0) return (flags & MASK_ZERO) == 0;  // not zero
        if (idx == 1) return (flags & MASK_ZERO);       // zero
        if (idx == 2) return (flags & MASK_CARRY) == 0; // not carry
        if (idx == 3) return (flags & MASK_CARRY);      // carry
        __builtin_unreachable();
    }

    inline static bool half_carry(const uint8_t reg, const uint8_t val)
    {
        return ((reg & 0xf) + (val & 0xf)) & (0x10);
    }
    inline static bool half_borrow(const uint8_t reg, const uint8_t val)
    {
        return (reg & 0xf) < (val & 0xf);
    }

    void alu(uint8_t op, uint8_t value) noexcept
    {
        auto& reg = this->accum;
        switch (op & 0x7)
        {
        case 0x0:
        { // ADD
            const uint16_t calc = reg + value;
            setflag(false, flags, MASK_NEGATIVE);
            setflag(half_carry(reg, value), flags, MASK_HALFCARRY);
            setflag(calc & 0x100, flags, MASK_CARRY);
            reg += value;
            setflag(reg == 0, flags, MASK_ZERO);
        }
            return;
        case 0x1:
        { // ADC
            const int carry = (flags & MASK_CARRY) ? 1 : 0;
            setflag(false, flags, MASK_NEGATIVE);
            setflag((reg & 0xf) + (value & 0xf) + carry > 0xf, flags, MASK_HALFCARRY); // annoying!
            setflag(((int) reg + value + carry) > 0xFF, flags, MASK_CARRY);
            reg += value + carry;
            setflag(reg == 0, flags, MASK_ZERO);
        }
            return;
        case 0x2: // SUB
            setflag(true, flags, MASK_NEGATIVE);
            setflag(half_borrow(reg, value), flags, MASK_HALFCARRY);
            setflag(reg < value, flags, MASK_CARRY);
            setflag(reg == value, flags, MASK_ZERO);
            reg -= value;
            return;
        case 0x3:
        { // SBC
            const int carry = (flags & MASK_CARRY) ? 1 : 0;
            flags = MASK_NEGATIVE;
            setflag(((reg & 0xf) - (value & 0xf) - carry) < 0, flags, MASK_HALFCARRY);
            setflag(reg < value + carry, flags, MASK_CARRY);
            reg -= value + carry;
            setflag(reg == 0, flags, MASK_ZERO);
        }
            return;
        case 0x4: // AND
            reg &= value;
            flags = MASK_HALFCARRY;
            setflag(reg == 0, flags, MASK_ZERO);
            return;
        case 0x5: // XOR
            reg ^= value;
            flags = 0;
            setflag(reg == 0, flags, MASK_ZERO);
            return;
        case 0x6: // OR
            reg |= value;
            flags = 0;
            setflag(reg == 0, flags, MASK_ZERO);
            return;
        case 0x7: // CP
            const uint8_t tmp = reg - value;
            flags |= MASK_NEGATIVE;
            setflag(tmp == 0, flags, MASK_ZERO);
            setflag(reg < value, flags, MASK_CARRY);
            setflag(half_borrow(reg, value), flags, MASK_HALFCARRY);
            // printf("CP %02x vs %02x (F => %02x)\n", reg, value, flags);
            return;
        }
    } // alu()

    std::string to_string() const
    {
        char buffer[512];
        int len = snprintf(buffer, sizeof(buffer),
                           "\tAF = %04X  BC = %04X  DE = %04X\n"
                           "\tHL = %04X  SP = %04X  PC = %04X\n",
                           af, bc, de, hl, sp, pc);
        return std::string(buffer, len);
    }
};

inline flags_t to_flag(const uint8_t opcode) { return (flags_t)(1 >> (4 + ((opcode >> 3) & 0x3))); }
} // namespace gbc
