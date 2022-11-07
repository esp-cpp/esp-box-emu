#include "apu.hpp"
#include "generators.hpp"
#include "io.hpp"
#include "machine.hpp"

namespace gbc
{
APU::APU(Machine& mach) : m_machine{mach} {}

void APU::simulate()
{
    // if sound is off, don't do anything
    if ((machine().io.reg(IO::REG_NR52) & 0x80) == 0) return;

    // TODO: writeme
}

uint8_t APU::read(const uint16_t addr, uint8_t& reg)
{
    switch (addr)
    {
    case IO::REG_NR52:
        return reg;
    }
    printf("ERROR: Unhandled APU read at %04X (reg %02X)\n", addr, reg);
    GBC_ASSERT(0 && "Unhandled APU read");
}
void APU::write(const uint16_t addr, const uint8_t value, uint8_t& reg)
{
    switch (addr)
    {
    case IO::REG_NR52:
        // TODO: writing bit7 should clear all sound registers
        // printf("NR52 Sound ON/OFF 0x%04x write 0x%02x\n", addr, value);
        reg &= 0xF;
        reg |= value & 0x80;
        // GBC_ASSERT(0 && "NR52 Sound ON/OFF register write");
        return;
    }
    printf("ERROR: Unhandled APU write at %04X val=%02X (reg %02X)\n", addr, value, reg);
    GBC_ASSERT(0 && "Unhandled APU write");
}

// serialization
int APU::restore_state(const std::vector<uint8_t>& data, int off)
{
    this->m_state = *(state_t*) &data.at(off);
    return sizeof(m_state);
}
void APU::serialize_state(std::vector<uint8_t>& res) const
{
    res.insert(res.end(), (uint8_t*) &m_state, (uint8_t*) &m_state + sizeof(m_state));
}
} // namespace gbc
