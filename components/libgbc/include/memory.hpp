#pragma once
#include "common.hpp"
#include "mbc.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gbc
{
class Memory
{
public:
    using range_t = std::pair<uint16_t, uint16_t>;
    static constexpr range_t ProgramArea{0x0000, 0x7FFF};
    static constexpr range_t VideoRAM{0x8000, 0x9FFF};

    static constexpr range_t BankRAM{0xA000, 0xBFFF};
    static constexpr range_t WorkRAM{0xC000, 0xDFFF};
    static constexpr range_t EchoRAM{0xE000, 0xFDFF}; // echo of work RAM
    static constexpr range_t OAM_RAM{0xFE00, 0xFEFF};

    static constexpr range_t IO_Ports{0xFF00, 0xFF7F};
    static constexpr range_t ZRAM{0xFF80, 0xFFFE};
    static constexpr uint16_t InterruptEn = 0xFFFF;

    Memory(Machine&, const std::string_view rom);
    void reset();
    void set_wram_bank(uint8_t bank);

    uint8_t read8(uint16_t address);
    void write8(uint16_t address, uint8_t value);

    uint16_t read16(uint16_t address);
    void write16(uint16_t address, uint16_t value);

    uint8_t* oam_ram_ptr() noexcept { return m_state.oam_ram.data(); }
    const uint8_t* oam_ram_ptr() const noexcept { return m_state.oam_ram.data(); }
    uint8_t* video_ram_ptr() noexcept { return m_state.video_ram.data(); }
    const uint8_t* video_ram_ptr() const noexcept { return m_state.video_ram.data(); }

    static constexpr uint16_t range_size(range_t range) { return range.second - range.first; }

    Machine& machine() const noexcept { return m_machine; }
    Machine& machine() noexcept { return m_machine; }
    bool rom_valid() const noexcept;
    bool bootrom_enabled() const noexcept { return false; }
    void disable_bootrom();

    bool double_speed() const noexcept { return m_state.speed_factor != 1; }
    int speed_factor() const noexcept { return m_state.speed_factor; }
    void do_switch_speed();

    // serialization
    int restore_state(const std::vector<uint8_t>&, int);
    void serialize_state(std::vector<uint8_t>&) const;

    // debugging
    std::string explain(uint16_t address) const;
    enum amode_t
    {
        READ,
        WRITE
    };
    using access_t = std::function<void(Memory&, uint16_t, uint8_t)>;
    void breakpoint(amode_t, access_t);

    inline static bool is_within(uint16_t addr, const range_t& range)
    {
        return addr >= range.first && addr <= range.second;
    }

private:
    Machine& m_machine;
    const std::string_view m_rom;
    MBC m_mbc;
    struct state_t
    {
        std::array<uint8_t, 16384> video_ram = {};
        std::array<uint8_t, 256> oam_ram = {};
        std::array<uint8_t, 128> zram = {}; // high-speed RAM
        bool bootrom_enabled = true;
        int8_t speed_factor = 1;
    } m_state;
    bool m_is_busy = false;
    std::vector<access_t> m_read_breakpoints;
    std::vector<access_t> m_write_breakpoints;
};

inline void Memory::breakpoint(amode_t mode, access_t func)
{
    if (mode == READ)
        m_read_breakpoints.push_back(func);
    else if (mode == WRITE)
        m_write_breakpoints.push_back(func);
}

inline uint16_t Memory::read16(uint16_t address)
{
    return read8(address) | read8(address + 1) << 8;
}
inline void Memory::write16(uint16_t address, uint16_t value)
{
    write8(address + 0, value & 0xff);
    write8(address + 1, value >> 8);
}

} // namespace gbc
