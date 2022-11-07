#pragma once
#include <cstdint>
#include <functional>

namespace gbc
{
class CPU;

struct breakpoint_t
{
    std::function<void(CPU&, uint8_t)> callback;
};

} // namespace gbc
