#pragma once
#include <cstdint>

namespace gbc
{
static const char* cstr_reg(const uint8_t bf, bool sp)
{
    static const char* notsp[] = {"BC", "DE", "HL", "AF"};
    static const char* notaf[] = {"BC", "DE", "HL", "SP"};
    if (sp) return notaf[(bf >> 4) & 0x3];
    return notsp[(bf >> 4) & 0x3];
}
static const char* cstr_dest(const uint8_t bf)
{
    static const char* dest[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
    return dest[bf & 0x7];
}
static const char* cstr_flags(char buff[5], const uint8_t flags)
{
    buff[0] = (flags & MASK_ZERO) ? 'Z' : '_';
    buff[1] = (flags & MASK_NEGATIVE) ? 'N' : '_';
    buff[2] = (flags & MASK_HALFCARRY) ? 'H' : '_';
    buff[3] = (flags & MASK_CARRY) ? 'C' : '_';
    buff[4] = 0;
    return buff;
}
static const char* cstr_cond(const uint8_t bf)
{
    static const char* s[] = {"not zero", "zero", "not carry", "carry"};
    return s[(bf >> 3) & 0x3];
}
static const char* cstr_cond_en(const uint8_t bf, const uint8_t flags)
{
    const uint8_t f = (bf >> 3) & 0x3;
    switch (f)
    {
    case 0:
        return (flags & 0x80) == 0 ? "YES" : "NO";
    case 1:
        return (flags & 0x80) ? "YES" : "NO";
    case 2:
        return (flags & 0x10) == 0 ? "YES" : "NO";
    case 3:
        return (flags & 0x10) ? "YES" : "NO";
    }
    __builtin_unreachable();
}
static void fill_flag_buffer(char* buffer, size_t len, const uint8_t opcode, const uint8_t flags)
{
    snprintf(buffer, len, "%s: %s", cstr_cond(opcode), cstr_cond_en(opcode, flags));
}

static const char* cstr_alu(const uint8_t bf)
{
    static const char* dest[] = {"ADD", "ADC", "SUB", "SBC", "AND", "XOR", "OR", "CP"};
    return dest[bf & 0x7];
}
} // namespace gbc
