#include "mbc.hpp"

#include "machine.hpp"
#include "memory.hpp"

namespace gbc
{
inline void MBC::write_MBC5(uint16_t addr, uint8_t value)
{
    switch (addr & 0xF000)
    {
    case 0x2000:
        // ROM bank select (lower)
        this->m_state.rom_bank_reg &= 0x100;
        this->m_state.rom_bank_reg |= value & 0xFF;
        this->set_rombank(this->m_state.rom_bank_reg);
        return;
    case 0x3000:
        // ROM bank select (upper)
        this->m_state.rom_bank_reg &= 0xFF;
        this->m_state.rom_bank_reg |= value & 0x100;
        this->set_rombank(this->m_state.rom_bank_reg);
        return;
    case 0x4000:
    case 0x5000:
        // RAM bank select
        this->set_rambank(value & 0xF);
        return;
    }
}
} // namespace gbc
