#pragma once
#include <string>

namespace gbc
{
class CPU;
using handler_t = void (*)(CPU&, uint8_t);
using printer_t = int (*)(char*, size_t, CPU&, uint8_t);

enum alu_t : uint8_t
{
    ADD = 0,
    ADC,
    SUB,
    SBC,
    AND,
    XOR,
    OR,
    CP
};

struct instruction_t
{
    const handler_t handler;
    const printer_t printer;
};
} // namespace gbc
