#include "memory.hpp"
#include "machine.hpp"

namespace gbc
{
Memory::Memory(Machine& mach, const std::string_view rom)
    : m_machine(mach), m_rom(rom), m_mbc{*this, rom}
{
    assert(this->rom_valid());
    this->disable_bootrom();
    m_mbc.init();
}
void Memory::reset()
{
    // this->disable_bootrom();
    // m_mbc.reset();
}
bool Memory::rom_valid() const noexcept
{
    // TODO: implement me
    return true;
}
void Memory::disable_bootrom() { m_state.bootrom_enabled = false; }

void Memory::set_wram_bank(uint8_t bank) { this->m_mbc.set_wrambank(bank); }

uint8_t Memory::read8(uint16_t address)
{
    if (UNLIKELY(!m_read_breakpoints.empty() && !m_is_busy))
    {
        this->m_is_busy = true;
        for (auto& func : m_read_breakpoints) { func(*this, address, 0x0); }
        this->m_is_busy = false;
    }
    switch (address & 0xF000)
    {
    case 0x0000:
    case 0x1000:
    case 0x2000:
    case 0x3000:
        return m_rom[address];
    case 0x4000:
    case 0x5000:
    case 0x6000:
    case 0x7000:
        address -= 0x4000;
        return m_rom[m_mbc.rombank_offset() | address];
    case 0x8000:
    case 0x9000:
        // cant read from Video RAM when working on scanline
        if (UNLIKELY(machine().gpu.get_mode() != 3))
        {
            const uint16_t offset = machine().gpu.video_offset();
            return m_state.video_ram.at(offset + address - VideoRAM.first);
        }
        return 0xff;
    case 0xA000:
    case 0xB000:
        return m_mbc.read(address);
    case 0xC000:
    case 0xD000:
        return m_mbc.read(address);
    case 0xE000: // echo RAM
        return m_mbc.read(address);
    case 0xF000:
        if (this->is_within(address, EchoRAM)) { return m_mbc.read(address); }
        else if (this->is_within(address, OAM_RAM))
        {
            // TODO: return 0xff when rendering?
            if (!machine().io.dma_active()) return m_state.oam_ram.at(address - OAM_RAM.first);
            return 0xff;
        }
        else if (this->is_within(address, IO_Ports))
        {
            return machine().io.read_io(address);
        }
        else if (this->is_within(address, ZRAM))
        {
            return m_state.zram.at(address - ZRAM.first);
        }
        else if (address == InterruptEn)
        {
            return machine().io.read_io(address);
        }
    }
    printf(">>> Invalid memory read at 0x%04x\n", address);
    return 0xff;
}

void Memory::write8(uint16_t address, uint8_t value)
{
    if (UNLIKELY(!m_write_breakpoints.empty() && !m_is_busy))
    {
        this->m_is_busy = true;
        for (auto& func : m_write_breakpoints) { func(*this, address, value); }
        this->m_is_busy = false;
    }
    switch (address & 0xF000)
    {
    case 0x0000:
    case 0x1000:
    case 0x2000:
    case 0x3000:
    case 0x4000:
    case 0x5000:
    case 0x6000:
    case 0x7000:
        m_mbc.write(address, value);
        return;
    case 0x8000:
    case 0x9000:
        if (machine().gpu.get_mode() != 3)
        {
            const uint16_t offset = machine().gpu.video_offset();
            m_state.video_ram.at(offset + address - VideoRAM.first) = value;
        }
        return;
    case 0xA000:
    case 0xB000:
        m_mbc.write(address, value);
        return;
    case 0xC000:
    case 0xD000:
        m_mbc.write(address, value);
        return;
    case 0xE000: // echo RAM
        m_mbc.write(address, value);
        return;
    case 0xF000:
        if (this->is_within(address, EchoRAM))
        {
            m_mbc.write(address, value);
            return;
        }
        else if (this->is_within(address, OAM_RAM))
        {
            this->m_state.oam_ram.at(address - OAM_RAM.first) = value;
            return;
        }
        else if (this->is_within(address, IO_Ports))
        {
            machine().io.write_io(address, value);
            return;
        }
        else if (this->is_within(address, ZRAM))
        {
            this->m_state.zram.at(address - ZRAM.first) = value;
            return;
        }
        else if (address == InterruptEn)
        {
            machine().io.write_io(address, value);
            return;
        }
    }
    printf(">>> Invalid memory write at 0x%04x, value 0x%x\n", address, value);
}

void Memory::do_switch_speed()
{
    auto& reg = machine().io.reg(IO::REG_KEY1);
    if (this->double_speed())
    {
        this->m_state.speed_factor = 1;
        reg = 0x0;
    }
    else
    {
        this->m_state.speed_factor = 2;
        reg = 0x80;
    }
}

std::string Memory::explain(const uint16_t addr) const
{
    switch (addr & 0xF000)
    {
    case 0x0000:
    case 0x1000:
    case 0x2000:
    case 0x3000:
        return "Program 0";
    case 0x4000:
    case 0x5000:
    case 0x6000:
    case 0x7000:
        return "Program " + std::to_string(m_mbc.rombank_offset() / 0x4000);
    case 0x8000:
    case 0x9000:
    {
        const uint16_t offset = machine().gpu.video_offset();
        return "VideoRAM " + std::to_string(offset / 0x2000);
    }
    case 0xA000:
    case 0xB000:
        return "ExtRAM";
    case 0xC000:
    case 0xD000:
        return "WorkRAM";
    case 0xE000:
        return "EchoRAM";
    case 0xF000:
        if (this->is_within(addr, EchoRAM)) return "EchoRAM";
        if (this->is_within(addr, OAM_RAM)) return "OAM RAM";
        if (this->is_within(addr, ZRAM)) return "ZRAM";
        return "I/O port";
    }
    return "Unknown";
}

// serialization
int Memory::restore_state(const std::vector<uint8_t>& data, int off)
{
    this->m_state = *(state_t*) &data.at(off);
    off += sizeof(state_t);
    // also restore MBC
    return sizeof(state_t) + this->m_mbc.restore_state(data, off);
}
void Memory::serialize_state(std::vector<uint8_t>& res) const
{
    res.insert(res.end(), (uint8_t*) &m_state, (uint8_t*) &m_state + sizeof(m_state));
    // also serialize MBC
    this->m_mbc.serialize_state(res);
}
} // namespace gbc
