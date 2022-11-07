#include "mbc.hpp"

#include "machine.hpp"
#include "memory.hpp"

namespace gbc
{
inline void MBC::write_MBC3(uint16_t addr, uint8_t value)
{
    switch (addr & 0xF000)
    {
    case 0x2000:
    case 0x3000:
        this->m_state.rom_bank_reg = value & 0x7F;
        if (m_state.rom_bank_reg == 0) m_state.rom_bank_reg = 1;
        this->set_rombank(this->m_state.rom_bank_reg);
        return;
    case 0x4000:
    case 0x5000:
        this->set_rambank(value & 0x7);
        this->m_state.rtc_enabled = (value & 0x80);
        return;
    case 0x6000:
    case 0x7000:
        // TODO: RTC latch values
        return;
    }
}
} // namespace gbc
