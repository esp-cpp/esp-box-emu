#include "machine.hpp"

namespace gbc
{
Machine::Machine(const std::string_view rom, bool init)
    : cpu(*this), memory(*this, rom), io(*this), gpu(*this), apu(*this)
{
    // set CGB mode when ROM supports it
    const uint8_t cgb = memory.read8(0x143);
    this->m_cgb_mode = (cgb & 0x80) && ENABLE_GBC;
    // reset CPU now that we know the machine type
    if (init) this->cpu.reset();
}

void Machine::reset()
{
    cpu.reset();
    memory.reset();
    io.reset();
    gpu.reset();
}
void Machine::stop() noexcept { this->m_running = false; }

void Machine::simulate_one_frame()
{
    while (gpu.current_scanline() != 0) { cpu.simulate(); }
    while (gpu.current_scanline() != 144) { cpu.simulate(); }
    assert(gpu.is_vblank());
}

uint64_t Machine::now() noexcept { return cpu.gettime(); }

void Machine::set_handler(interrupt i, interrupt_handler handler)
{
    switch (i)
    {
    case VBLANK:
        io.vblank.callback = handler;
        return;
    case TIMER:
        io.timerint.callback = handler;
        return;
    case JOYPAD:
        io.joypadint.callback = handler;
        return;
    case DEBUG:
        io.debugint.callback = handler;
        return;
    }
}

void Machine::set_inputs(uint8_t mask) { io.trigger_keys(mask); }

size_t Machine::restore_state(const std::vector<uint8_t>& data)
{
    int offset = 0;
    offset += cpu.restore_state(data, offset);
    offset += memory.restore_state(data, offset);
    offset += io.restore_state(data, offset);
    offset += gpu.restore_state(data, offset);
    offset += apu.restore_state(data, offset);
    return offset;
}
void Machine::serialize_state(std::vector<uint8_t>& result) const
{
    cpu.serialize_state(result);
    memory.serialize_state(result);
    io.serialize_state(result);
    gpu.serialize_state(result);
    apu.serialize_state(result);
}

void Machine::break_now() { cpu.break_now(); }
bool Machine::is_breaking() const noexcept { return cpu.is_breaking(); }

void Machine::undefined()
{
    if (this->stop_when_undefined)
    {
        printf("*** An undefined operation happened\n");
        cpu.break_now();
    }
}
} // namespace gbc
