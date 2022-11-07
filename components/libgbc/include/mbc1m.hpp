#include "mbc.hpp"

#include "machine.hpp"
#include "memory.hpp"

namespace gbc
{
inline void MBC::write_MBC1M(uint16_t addr, uint8_t value)
{
    switch (addr & 0xF000)
    {
    case 0x2000:
    case 0x3000:
        // ROM bank number
        this->m_state.rom_bank_reg &= 0x60;
        this->m_state.rom_bank_reg |= value & 0x1F;
        // lower 5 bits cant be 0
        if ((value & 0x1F) == 0) { this->m_state.rom_bank_reg++; }
        this->set_rombank(this->m_state.rom_bank_reg);
        return;
    case 0x4000:
    case 0x5000:
        // ROM / RAM bank select
        if (this->m_state.mode_select == 1) { this->set_rambank(value & 0x3); }
        // always changed ROM bank value
        this->m_state.rom_bank_reg &= 0x1F;
        this->m_state.rom_bank_reg |= (value & 0x3) << 5;
        this->set_rombank(this->m_state.rom_bank_reg);
        return;
    case 0x6000:
    case 0x7000:
        // RAM / ROM mode select
        this->set_mode(value & 0x1);
    }
}
} // namespace gbc
