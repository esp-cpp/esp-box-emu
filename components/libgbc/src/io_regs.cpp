#include "machine.hpp"
// should only be included once
#define IOHANDLER(off, x) new (&iologic.at(off - 0xff00)) iowrite_t{iowrite_##x, ioread_##x};

namespace gbc
{
struct iowrite_t
{
    using write_handler_t = void (*)(IO&, uint16_t, uint8_t);
    using read_handler_t = uint8_t (*)(IO&, uint16_t);
    const write_handler_t on_write = nullptr;
    const read_handler_t on_read = nullptr;
};
static std::array<iowrite_t, 128> iologic = {};

void iowrite_JOYP(IO& io, uint16_t, uint8_t value)
{
    if (value & 0x10)
        io.joypad().ioswitch = 0;
    else if (value & 0x20)
        io.joypad().ioswitch = 1;
}
uint8_t ioread_JOYP(IO& io, uint16_t)
{
    io.trigger_joypad_read();
    switch (io.joypad().ioswitch)
    {
    case 0:
        return 0xD0 | io.joypad().buttons;
    case 1:
        return 0xE0 | io.joypad().keypad;
    }
    GBC_ASSERT(0 && "Invalid joypad GPIO value");
}

void iowrite_DIV(IO& io, uint16_t, uint8_t)
{
    // writing to DIV resets it to 0
    io.reset_divider();
}
uint8_t ioread_DIV(IO& io, uint16_t) { return io.reg(IO::REG_DIV); }

void iowrite_LCDC(IO& io, uint16_t addr, uint8_t value)
{
    const bool was_enabled = io.reg(addr) & 0x80;
    io.reg(addr) = value;
    const bool is_enabled = io.reg(addr) & 0x80;
    // check if LCD just turned on
    if (!was_enabled && is_enabled) { io.machine().gpu.lcd_power_changed(true); }
    // check if LCD was just turned off
    else if (was_enabled && !is_enabled)
    {
        io.machine().gpu.lcd_power_changed(false);
    }
}
uint8_t ioread_LCDC(IO& io, uint16_t addr) { return io.reg(addr); }

void iowrite_STAT(IO& io, uint16_t addr, uint8_t value)
{
    // can only write to the upper bits 3-6
    io.reg(addr) &= 0x87;
    io.reg(addr) |= value & 0x78;
}
uint8_t ioread_STAT(IO& io, uint16_t addr) { return io.reg(addr) | 0x80; }

void iowrite_DMA(IO& io, uint16_t, uint8_t value)
{
    const uint16_t src = value << 8;
    // printf("DMA copy start from 0x%04x to 0x%04x\n", src, dst);
    io.start_dma(src);
}
uint8_t ioread_DMA(IO& io, uint16_t addr) { return io.reg(addr); }

void iowrite_HDMA(IO& io, uint16_t addr, uint8_t value)
{
    if (io.machine().is_cgb() == false) return;
    switch (addr)
    {
    case IO::REG_HDMA1:
    case IO::REG_HDMA3:
        io.reg(addr) = value;
        return;
    case IO::REG_HDMA2:
    case IO::REG_HDMA4:
        io.reg(addr) = value & 0xF0;
        return;
    }
    // HDMA 5: start DMA operation
    uint16_t src = (io.reg(IO::REG_HDMA1) << 8) | io.reg(IO::REG_HDMA2);
    src &= 0xFFF0;
    uint16_t dst = (io.reg(IO::REG_HDMA3) << 8) | io.reg(IO::REG_HDMA4);
    dst &= 0x9FF0;
    dst |= 0x8000; // VRAM only!
    // length is measured blocks of 16-bytes, minimum 16
    const uint16_t num_bytes = (1 + (value & 0x7F)) * 16;

    if ((value & 0x80) == 0)
    {
        if (io.hdma_active())
        {
            // disable currently running HDMA
            io.start_hdma(0, 0, 0);
            io.reg(IO::REG_HDMA5) = value;
        }
        else
        {
            // do the transfer immediately
            // printf("HDMA transfer 0x%04x to 0x%04x (%u bytes)\n", src, dst, end - src);
            const uint16_t end = src + num_bytes;
            auto& mem = io.machine().memory;
            while (src < end) mem.write8(dst++, mem.read8(src++));
            // transfer complete
            io.reg(IO::REG_HDMA5) = 0xFF;
        }
    }
    else
    {
        // H-blank DMA
        io.start_hdma(src, dst, num_bytes);
        io.reg(IO::REG_HDMA5) = value;
    }
}
uint8_t ioread_HDMA(IO& io, uint16_t addr)
{
    switch (addr)
    {
    case IO::REG_HDMA1:
    case IO::REG_HDMA3:
    case IO::REG_HDMA2:
    case IO::REG_HDMA4:
        return 0xFF; // apparently always?
    case IO::REG_HDMA5:
        return io.reg(addr);
    }
    return 0xFF;
}

void iowrite_AUDIO(IO& io, uint16_t addr, uint8_t value)
{
    io.machine().apu.write(addr, value, io.reg(addr));
}
uint8_t ioread_AUDIO(IO& io, uint16_t addr) { return io.machine().apu.read(addr, io.reg(addr)); }

void iowrite_KEY1(IO& io, uint16_t addr, uint8_t value)
{
    // printf("KEY1 0x%04x write 0x%02x\n", addr, value);
    io.reg(addr) &= 0x80;
    io.reg(addr) |= value & 1;
}
uint8_t ioread_KEY1(IO& io, uint16_t addr)
{
    if (!io.machine().is_cgb()) return 0xff;
    return (io.reg(addr) & 0x81) | 0x7E;
}

void iowrite_VBK(IO& io, uint16_t addr, uint8_t value)
{
    io.reg(addr) = value & 1;
    io.machine().gpu.set_video_bank(value & 1);
}
uint8_t ioread_VBK(IO& io, uint16_t addr) { return io.reg(addr) | 0xfe; }

void iowrite_BOOT(IO& io, uint16_t addr, uint8_t value)
{
    if (value) { io.machine().memory.disable_bootrom(); }
    io.reg(addr) |= value;
}
uint8_t ioread_BOOT(IO& io, uint16_t addr) { return io.reg(addr); }

void iowrite_SVBK(IO& io, uint16_t addr, uint8_t value)
{
    // printf("SVBK 0x%04x write 0x%02x\n", addr, value);
    value &= 0x7;
    if (value == 0) value = 1;
    io.reg(addr) = value;
    io.machine().memory.set_wram_bank(value);
}
uint8_t ioread_SVBK(IO& io, uint16_t addr) { return io.reg(addr); }

inline void auto_increment(uint8_t& idx, const uint8_t mask)
{
    const uint8_t v = idx & mask;
    idx &= ~mask;
    idx |= (v + 1) & mask;
}

void iowrite_BGPD(IO& io, uint16_t, uint8_t value)
{
    uint8_t& idx = io.reg(IO::REG_BGPI); // palette index
    io.machine().gpu.setpal(0 + (idx & 63), value);
    // when bit7 is set, auto-increment the index register
    if (idx & 0x80) auto_increment(idx, 63);
}
uint8_t ioread_BGPD(IO& io, uint16_t)
{
    uint8_t idx = io.reg(IO::REG_BGPI) & 63; // palette index
    return io.machine().gpu.getpal(0 + idx);
}

void iowrite_OBPD(IO& io, uint16_t, uint8_t value)
{
    uint8_t& idx = io.reg(IO::REG_OBPI); // palette index
    io.machine().gpu.setpal(64 + (idx & 63), value);
    // when bit7 is set, auto-increment the index register
    if (idx & 0x80) auto_increment(idx, 63);
}
uint8_t ioread_OBPD(IO& io, uint16_t)
{
    uint8_t idx = io.reg(IO::REG_OBPI) & 63; // palette index
    return io.machine().gpu.getpal(64 + idx);
}

__attribute__((constructor)) static void set_io_handlers()
{
    IOHANDLER(IO::REG_P1, JOYP);
    IOHANDLER(IO::REG_DIV, DIV);
    IOHANDLER(IO::REG_LCDC, LCDC);
    IOHANDLER(IO::REG_STAT, STAT);
    IOHANDLER(IO::REG_DMA, DMA);
    IOHANDLER(IO::REG_NR52, AUDIO);
    // CGB registers
    IOHANDLER(IO::REG_KEY1, KEY1);
    IOHANDLER(IO::REG_VBK, VBK);
    IOHANDLER(IO::REG_SVBK, SVBK);
    IOHANDLER(IO::REG_BOOT, BOOT);
    IOHANDLER(IO::REG_HDMA1, HDMA);
    IOHANDLER(IO::REG_HDMA2, HDMA);
    IOHANDLER(IO::REG_HDMA3, HDMA);
    IOHANDLER(IO::REG_HDMA4, HDMA);
    IOHANDLER(IO::REG_HDMA5, HDMA);
    // CGB palettes
    IOHANDLER(IO::REG_BGPD, BGPD);
    IOHANDLER(IO::REG_OBPD, OBPD);
}
} // namespace gbc
