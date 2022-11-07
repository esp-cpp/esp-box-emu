#pragma once
#include "common.hpp"
#include "interrupt.hpp"
#include <array>
#include <cstdint>

namespace gbc
{
class IO
{
public:
    static const uint16_t SND_START = 0xff10;
    static const uint16_t SND_END = 0xff40;
    enum regnames_t
    {
        REG_P1 = 0xff00,
        // TIMER
        REG_DIV = 0xff04,
        REG_TIMA = 0xff05,
        REG_TMA = 0xff06,
        REG_TAC = 0xff07,
        // SOUND
        REG_NR10 = 0xff10,
        REG_NR11 = 0xff11,
        REG_NR12 = 0xff12,
        REG_NR13 = 0xff13,
        REG_NR14 = 0xff14,
        REG_NR21 = 0xff16,
        REG_NR22 = 0xff17,
        REG_NR23 = 0xff18,
        REG_NR24 = 0xff19,
        REG_NR30 = 0xff1a,
        REG_NR31 = 0xff1b,
        REG_NR32 = 0xff1c,
        REG_NR33 = 0xff1d,
        REG_NR34 = 0xff1e,
        REG_NR41 = 0xff20,
        REG_NR42 = 0xff21,
        REG_NR43 = 0xff22,
        REG_NR44 = 0xff23,
        REG_NR50 = 0xff24,
        REG_NR51 = 0xff25,
        REG_NR52 = 0xff26,
        REG_WAV0 = 0xff30,
        REG_WAVF = 0xff3f,
        // LCD
        REG_LCDC = 0xff40,
        REG_STAT = 0xff41,
        REG_SCY = 0xff42,
        REG_SCX = 0xff43,
        REG_LY = 0xff44,
        REG_LYC = 0xff45,
        REG_DMA = 0xff46,
        // PALETTE
        REG_BGP = 0xff47,
        REG_OBP0 = 0xff48,
        REG_OBP1 = 0xff49,
        REG_WY = 0xff4a,
        REG_WX = 0xff4b,

        // CBG I/O regs
        REG_KEY1 = 0xff4d,
        REG_VBK = 0xff4f,
        REG_BOOT = 0xff50,

        REG_HDMA1 = 0xff51,
        REG_HDMA2 = 0xff52,
        REG_HDMA3 = 0xff53,
        REG_HDMA4 = 0xff54,
        REG_HDMA5 = 0xff55,

        REG_BGPI = 0xff68,
        REG_BGPD = 0xff69,
        REG_OBPI = 0xff6a,
        REG_OBPD = 0xff6b,

        REG_SVBK = 0xff70,

        // INTERRUPTS
        REG_IF = 0xff0f,
        REG_IE = 0xffff,
    };

    IO(Machine&);
    void write_io(const uint16_t, uint8_t);
    uint8_t read_io(const uint16_t);

    void trigger_keys(uint8_t);
    bool joypad_is_disabled() const noexcept;
    void trigger(interrupt_t&);
    uint8_t interrupt_mask() const;
    void start_dma(uint16_t src);
    void start_hdma(uint16_t src, uint16_t dst, uint16_t bytes);
    bool dma_active() const noexcept { return oam_dma().bytes_left > 0; }
    bool hdma_active() const noexcept { return hdma().bytes_left > 0; }

    void perform_stop();
    void deactivate_stop();
    void reset_divider();

    Machine& machine() noexcept { return m_machine; }

    void reset();
    void simulate();

    inline uint8_t& reg(const uint16_t addr) { return m_state.ioregs[addr & 0x7f]; }
    inline const uint8_t& reg(const uint16_t addr) const { return m_state.ioregs[addr & 0x7f]; }

    struct joypad_t
    {
        uint8_t ioswitch = 0;
        uint8_t keypad = 0xFF;
        uint8_t buttons = 0xFF;
        uint8_t last_mask = 0;
    };
    inline joypad_t& joypad() { return m_state.joypad; }

    using joypad_read_handler_t = std::function<void(Machine&, int)>;
    void on_joypad_read(joypad_read_handler_t h) { m_jp_handler = h; }
    void trigger_joypad_read()
    {
        if (m_jp_handler) m_jp_handler(machine(), joypad().ioswitch);
    }

    interrupt_t vblank;
    interrupt_t lcd_stat;
    interrupt_t timerint;
    interrupt_t serialint;
    interrupt_t joypadint;
    interrupt_t debugint;

    // serialization
    int restore_state(const std::vector<uint8_t>&, int);
    void serialize_state(std::vector<uint8_t>&) const;

private:
    struct dma_t
    {
        uint64_t cur_line;
        int8_t slow_start = 0;
        uint16_t src;
        uint16_t dst;
        int32_t bytes_left = 0;
    };
    const dma_t& oam_dma() const noexcept { return m_state.dma; }
    dma_t& oam_dma() noexcept { return m_state.dma; }
    const dma_t& hdma() const noexcept { return m_state.hdma; }
    dma_t& hdma() noexcept { return m_state.hdma; }

    Machine& m_machine;
    struct state_t
    {
        std::array<uint8_t, 128> ioregs = {};
        joypad_t joypad;
        uint16_t divider = 0;
        uint16_t timabug = 0;
        // LCD on/off during STOP?
        bool lcd_powered = false;
        uint8_t reg_ie = 0x0;

        dma_t dma;
        dma_t hdma;
    } m_state;

    joypad_read_handler_t m_jp_handler = nullptr;
};

inline void IO::trigger(interrupt_t& intr) { this->reg(REG_IF) |= intr.mask; }
inline uint8_t IO::interrupt_mask() const { return this->m_state.reg_ie & this->reg(REG_IF); }
} // namespace gbc
