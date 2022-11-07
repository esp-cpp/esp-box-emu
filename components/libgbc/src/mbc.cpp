#include "mbc.hpp"

#include "machine.hpp"
#include "memory.hpp"

#include "mbc1m.hpp"
#include "mbc3.hpp"
#include "mbc5.hpp"

namespace gbc
{
MBC::MBC(Memory& m, const std::string_view rom)
    : m_memory(m), m_rom(rom) {}

void MBC::init()
{
    this->m_ram.at(0x100) = 0x1;
    this->m_ram.at(0x101) = 0x3;
    this->m_ram.at(0x102) = 0x5;
    this->m_ram.at(0x103) = 0x7;
    this->m_ram.at(0x104) = 0x9;
    // test ROMs are just instruction arrays
    if (m_rom.size() < 0x150) return;
    // parse ROM header
    switch (m_memory.read8(0x147))
    {
    case 0x0:
    case 0x1: // MBC 1
    case 0x2:
    case 0x3:
        this->m_state.version = 1;
        break;
    case 0x5:
    case 0x6:
        this->m_state.version = 2;
        assert(0 && "MBC2 is a weirdo!");
        break;
    case 0x0F:
    case 0x10: // MBC 3
    case 0x12:
    case 0x13:
        this->m_state.version = 3;
        break;
    case 0x19:
    case 0x1A: // MBC 5
    case 0x1B:
    case 0x1C:
        this->m_state.version = 5;
        this->m_state.rumble = false;
        break;
    case 0x1D:
    case 0x1E:
        this->m_state.version = 5;
        this->m_state.rumble = true;
        break;
    default:
        assert(0 && "Unknown cartridge type");
    }
    // printf("MBC version %u  Rumble: %d\n", this->m_state.version, this->m_state.rumble);
    switch (m_memory.read8(0x149))
    {
    case 0x0:
        m_state.ram_banks = 0;
        m_state.ram_bank_size = 0;
        break;
    case 0x1: // 2kb
        m_state.ram_banks = 1;
        m_state.ram_bank_size = 2048;
        break;
    case 0x2: // 8kb
        m_state.ram_banks = 1;
        m_state.ram_bank_size = 8192;
        break;
    case 0x3: // 32kb
        m_state.ram_banks = 4;
        m_state.ram_bank_size = 32768;
        break;
    case 0x4: // 128kb
        m_state.ram_banks = 16;
        m_state.ram_bank_size = 0x20000;
        break;
    case 0x5: // 64kb
        m_state.ram_banks = 8;
        m_state.ram_bank_size = 0x10000;
        break;
    }
    // printf("RAM bank size: 0x%05x\n", m_state.ram_bank_size);
    this->m_state.wram_size = 0x8000;
    // printf("Work RAM bank size: 0x%04x\n", m_state.wram_size);
}

uint8_t MBC::read(uint16_t addr)
{
    switch (addr & 0xF000)
    {
    case 0xA000:
    case 0xB000:
        if (this->ram_enabled())
        {
            if (this->m_state.rtc_enabled == false)
            {
                addr -= RAMbankX.first;
                addr |= this->m_state.ram_bank_offset;
                if (addr < this->m_state.ram_bank_size) return this->m_ram.at(addr);
                return 0xff; // small 2kb RAM banks
            }
            else
            {
                return 0xff; // TODO: Read from RTC register
            }
        }
        else
        {
            return 0xff;
        }
    case 0xC000:
        return this->m_state.wram.at(addr - WRAM_0.first);
    case 0xD000:
        return m_state.wram.at(m_state.wram_offset + addr - WRAM_bX.first);
    case 0xE000: // echo RAM
    case 0xF000:
        return this->read(addr - 0x2000);
    }
    printf("* Invalid MBC read: 0x%04x\n", addr);
    return 0xff;
}

void MBC::write(uint16_t addr, uint8_t value)
{
    switch (addr & 0xF000)
    {
    case 0x0000:
    case 0x1000:
        // RAM enable
        if (m_state.version == 2)
            this->m_state.ram_enabled = value != 0;
        else
            this->m_state.ram_enabled = ((value & 0xF) == 0xA);
        if (UNLIKELY(verbose_banking())) {
            printf("* External RAM enabled: %d\n", this->m_state.ram_enabled);
        }
        return;
    case 0x2000:
    case 0x3000:
    case 0x4000:
    case 0x5000:
    case 0x6000:
    case 0x7000:
        // MBC control ranges
        switch (this->m_state.version)
        {
        case 5:
            this->write_MBC5(addr, value);
            break;
        case 3:
            this->write_MBC3(addr, value);
            break;
        case 1:
            this->write_MBC1M(addr, value);
            break;
        case 0:
            break; // no MBC
        default:
            assert(0 && "Unimplemented MBC version");
        }
        return;
    case 0xA000:
    case 0xB000:
        if (this->ram_enabled())
        {
            if (this->m_state.rtc_enabled == false)
            {
                addr -= RAMbankX.first;
                addr |= this->m_state.ram_bank_offset;
                if (addr < this->m_state.ram_bank_size) { this->m_ram.at(addr) = value; }
            }
            else
            {
                // TODO: Write to RTC register
            }
        }
        return;
    case 0xC000: // WRAM bank 0
        this->m_state.wram.at(addr - WRAM_0.first) = value;
        return;
    case 0xD000: // WRAM bank X
        this->m_state.wram.at(m_state.wram_offset + addr - WRAM_bX.first) = value;
        return;
    case 0xE000: // Echo RAM
    case 0xF000:
        this->write(addr - 0x2000, value);
        return;
    }
    printf("* Invalid MBC write: 0x%04x => 0x%02x\n", addr, value);
    assert(0);
}

void MBC::set_rombank(int reg)
{
    const int rom_banks = m_rom.size() / rombank_size();
    reg &= (rom_banks - 1);

    // cant select bank 0
    const int offset = reg * rombank_size();
    if (UNLIKELY(verbose_banking())) {
        printf("Selecting ROM bank 0x%02x offset %#x max %#zx\n", reg, offset, m_rom.size());
    }
    if (UNLIKELY((offset + rombank_size()) > m_rom.size()))
    {
        printf("Invalid ROM bank 0x%02x offset %#x max %#zx\n", reg, offset, m_rom.size());
        this->m_memory.machine().break_now();
        return;
    }
    this->m_state.rom_bank_offset = offset;
}
void MBC::set_rambank(int reg)
{
    if (this->m_state.ram_banks >= 0)
    {
        // NOTE: we have to remove bits here
        reg &= (this->m_state.ram_banks - 1);
    }
    const int offset = reg * rambank_size();
    if (UNLIKELY(verbose_banking())) {
        printf("Selecting RAM bank 0x%02x offset %#x max %#lx\n", reg, offset,
               m_state.ram_bank_size);
    }
    this->m_state.ram_bank_offset = offset;
}
void MBC::set_wrambank(int reg)
{
    const int offset = reg * wrambank_size();
    if (UNLIKELY(verbose_banking())) {
        printf("Selecting WRAM bank 0x%02x offset %#x max %#x\n", reg, offset, m_state.wram_size);
    }
    if (UNLIKELY((offset + wrambank_size()) > m_state.wram_size))
    {
        printf("Invalid Work RAM bank 0x%02x offset %#x\n", reg, offset);
        this->m_memory.machine().break_now();
        return;
    }
    this->m_state.wram_offset = offset;
}
void MBC::set_mode(int mode)
{
    if (UNLIKELY(verbose_banking())) {
        printf("Mode select: 0x%02x\n", this->m_state.mode_select);
    }
    this->m_state.mode_select = mode & 0x1;
    // for MBC we have to reset the upper bits when going into RAM mode
    if (this->m_state.version == 1 && this->m_state.mode_select == 1)
    {
        // reset ROM bank upper bits when going into RAM mode
        if (this->m_state.rom_bank_reg & 0x60)
        {
            this->m_state.rom_bank_reg &= 0x1F;
            this->set_rombank(this->m_state.rom_bank_reg);
        }
    }
}

bool MBC::verbose_banking() const noexcept { return m_memory.machine().verbose_banking; }

// serialization
int MBC::restore_state(const std::vector<uint8_t>& data, int off)
{
    // copy state first
    this->m_state = *(state_t*) &data.at(off);
    off += sizeof(state_t);
    // then copy RAM by size
    std::copy(&data.at(off), &data.at(off) + m_state.ram_bank_size, m_ram.begin());
    return sizeof(state_t) + m_state.ram_bank_size;
}
void MBC::serialize_state(std::vector<uint8_t>& res) const
{
    res.insert(res.end(), (uint8_t*) &m_state, (uint8_t*) &m_state + sizeof(m_state));
    res.insert(res.end(), m_ram.begin(), m_ram.begin() + m_state.ram_bank_size);
}
} // namespace gbc
