#include "io.hpp"
#include "io_regs.cpp"
#include "machine.hpp"
#include <cstdio>

namespace gbc
{
IO::IO(Machine& mach)
    : vblank{0x1, 0x40, "V-blank"}
    , lcd_stat{0x2, 0x48, "LCD Status"}
    , timerint{0x4, 0x50, "Timer"}
    , serialint{0x8, 0x58, "Serial"}
    , joypadint{0x10, 0x60, "Joypad"}
    , debugint{0x0, 0x0, "Debug"}
    , m_machine(mach)
{
    this->reset();
}

void IO::reset()
{
    // register defaults
    reg(REG_P1) = 0xcf;
    reg(REG_TIMA) = 0x00;
    reg(REG_TMA) = 0x00;
    reg(REG_TAC) = 0xf8;
    // sound defaults
    reg(REG_NR10) = 0x80;
    reg(REG_NR11) = 0xbf;
    reg(REG_NR52) = 0xf1;
    // LCD defaults
    reg(REG_LCDC) = 0x91;
    reg(REG_STAT) = 0x81;
    reg(REG_LY) = 0x90; // 144
    reg(REG_LYC) = 0x0;
    reg(REG_DMA) = 0x00;
    // Palette
    reg(REG_BGP) = 0xfc;
    reg(REG_OBP0) = 0xff;
    reg(REG_OBP1) = 0xff;
    // boot rom enabled at boot
    reg(REG_BOOT) = 0x00;
    reg(REG_HDMA5) = 0xFF;

    this->m_state.reg_ie = 0x00;
}

void IO::simulate()
{
    // 1. DIV timer
    this->m_state.divider += 256 / 64;
    this->reg(REG_DIV) = this->m_state.divider >> 8;

    // 2. TIMA timer
    if (this->reg(REG_TAC) & 0x4)
    {
        const std::array<int, 4> TIMA_CYCLES = {1024, 16, 64, 256};
        const int speed = this->reg(REG_TAC) & 0x3;
        // TIMA counter timer
        if (m_state.divider % (TIMA_CYCLES[speed]) == 0)
        {
            this->reg(REG_TIMA)++;
            // if (reg(REG_TIMA) > 16) machine().break_now();
            // timer interrupt when overflowing to 0
            if (this->reg(REG_TIMA) == 0)
            {
                this->trigger(this->timerint);
                // BUG: TIMA does not get reset before after 4 cycles
                this->m_state.timabug = 4;
            }
        }
        else if (UNLIKELY(this->m_state.timabug > 0))
        {
            this->m_state.timabug--;
            if (this->m_state.timabug == 0)
            {
                // restart at modulo
                this->reg(REG_TIMA) = this->reg(REG_TMA);
            }
        }
    }

    // 3. OAM DMA operation
    if (this->m_state.dma.bytes_left > 0)
    {
        if (this->m_state.dma.slow_start > 0) { this->m_state.dma.slow_start--; }
        else
        {
            // calculate number of bytes to copy
            const int btw = 1;
            // do the copying
            auto& memory = machine().memory;
            for (int i = 0; i < btw; i++)
            { memory.write8(m_state.dma.dst++, memory.read8(m_state.dma.src++)); }
            assert(m_state.dma.bytes_left >= btw);
            m_state.dma.bytes_left -= btw;
        }
    }

    // 4. HDMA operation
    if (this->hdma().bytes_left > 0)
    {
        // during H-blank, once for each line
        if (machine().gpu.is_hblank() && hdma().cur_line != reg(REG_LY))
        {
            hdma().cur_line = reg(REG_LY);
            auto& memory = machine().memory;
            int btw = std::min(16, (int)(hdma().bytes_left));
            // do the copying
            for (int i = 0; i < btw; i++)
            { memory.write8(hdma().dst++, memory.read8(hdma().src++)); }
            hdma().dst &= 0x9FFF; // make sure it wraps around VRAM
            assert(hdma().bytes_left >= btw);
            hdma().bytes_left -= btw;
            if (hdma().bytes_left == 0)
            {
                // transfer complete
                this->reg(REG_HDMA5) = 0xFF;
            }
        }
    }
}

uint8_t IO::read_io(const uint16_t addr)
{
    // default: just return the register value
    if (addr >= 0xff00 && addr < 0xff80)
    {
        if (UNLIKELY(machine().break_on_io && !machine().is_breaking()))
        {
            printf("[io] * I/O read 0x%04x => 0x%02x\n", addr, reg(addr));
            machine().break_now();
        }
        auto& handler = iologic.at(addr - 0xff00);
        if (handler.on_read != nullptr) { return handler.on_read(*this, addr); }
        return reg(addr);
    }
    if (addr == REG_IE) { return this->m_state.reg_ie; }
    printf("[io] * Unknown read 0x%04x\n", addr);
    machine().undefined();
    return 0xff;
}
void IO::write_io(const uint16_t addr, uint8_t value)
{
    // default: just write to register
    if (addr >= 0xff00 && addr < 0xff80)
    {
        if (UNLIKELY(machine().break_on_io && !machine().is_breaking()))
        {
            printf("[io] * I/O write 0x%04x value 0x%02x\n", addr, value);
            machine().break_now();
        }
        auto& handler = iologic.at(addr - 0xff00);
        if (handler.on_write != nullptr)
        {
            handler.on_write(*this, addr, value);
            return;
        }
        // default: just write...
        reg(addr) = value;
        return;
    }
    if (addr == REG_IE)
    {
        this->m_state.reg_ie = value;
        return;
    }
    printf("[io] * Unknown write 0x%04x value 0x%02x\n", addr, value);
    machine().undefined();
}

void IO::trigger_keys(uint8_t mask)
{
    joypad().keypad = ~(mask & 0xF);
    joypad().buttons = ~(mask >> 4);
    // trigger joypad interrupt on every change
    if (joypad().last_mask != mask)
    {
        joypad().last_mask = mask;
        this->trigger(joypadint);
    }
}
bool IO::joypad_is_disabled() const noexcept { return (reg(REG_P1) & 0x30) == 0x30; }

void IO::start_dma(uint16_t src)
{
    oam_dma().slow_start = 2;
    oam_dma().src = src;
    oam_dma().dst = 0xfe00;
    oam_dma().bytes_left = 160; // 160 bytes total
}

void IO::start_hdma(uint16_t src, uint16_t dst, uint16_t bytes)
{
    hdma().src = src;
    hdma().dst = dst;
    hdma().bytes_left = bytes;
    hdma().cur_line = 0xff;
}

void IO::perform_stop()
{
    // bit 1 = stopped, bit 8 = LCD on/off
    this->reg(REG_KEY1) = 0x1;
    // remember previous LCD on/off value
    this->m_state.lcd_powered = reg(REG_LCDC) & 0x80;
    // disable LCD
    reg(REG_LCDC) &= ~0x80;
    // enable joypad interrupts
    this->m_state.reg_ie |= joypadint.mask;
}
void IO::deactivate_stop()
{
    // turn screen back on, if it was turned off
    if (this->m_state.lcd_powered) reg(REG_LCDC) |= 0x80;
    reg(REG_KEY1) = machine().memory.double_speed() ? 0x80 : 0x0;
}

void IO::reset_divider()
{
    this->m_state.divider = 0;
    this->reg(REG_DIV) = 0;
}

int IO::restore_state(const std::vector<uint8_t>& data, int off)
{
    this->m_state = *(state_t*) &data.at(off);
    return sizeof(m_state);
}
void IO::serialize_state(std::vector<uint8_t>& res) const
{
    res.insert(res.end(), (uint8_t*) &m_state, (uint8_t*) &m_state + sizeof(m_state));
}
} // namespace gbc
