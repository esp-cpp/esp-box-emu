#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace gbc
{
class Memory;

class MBC
{
public:
    using range_t = std::pair<uint16_t, uint16_t>;
    static constexpr range_t ROMbank0{0x0000, 0x4000};
    static constexpr range_t ROMbankX{0x4000, 0x8000};
    static constexpr range_t RAMbankX{0xA000, 0xC000};
    static constexpr range_t WRAM_0{0xC000, 0xD000};
    static constexpr range_t WRAM_bX{0xD000, 0xE000};
    static constexpr range_t EchoRAM{0xE000, 0xFE00};

    MBC(Memory&, const std::string_view rom);

    const auto& rom() const noexcept { return m_rom; }
    uint32_t rombank_offset() const noexcept { return m_state.rom_bank_offset; }

    bool ram_enabled() const noexcept { return m_state.ram_enabled; }
    size_t rombank_size() const noexcept { return 0x4000; }
    size_t rambank_size() const noexcept { return 0x2000; }
    size_t wrambank_size() const noexcept { return 0x1000; }

    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t value);

    void set_rombank(int offset);
    void set_rambank(int offset);
    void set_wrambank(int offset);
    void set_mode(int mode);

    // serialization
    int restore_state(const std::vector<uint8_t>&, int);
    void serialize_state(std::vector<uint8_t>&) const;

private:
    void write_MBC1M(uint16_t, uint8_t);
    void write_MBC3(uint16_t, uint8_t);
    void write_MBC5(uint16_t, uint8_t);
    bool verbose_banking() const noexcept;

    Memory& m_memory;
    const std::string_view m_rom;
    struct state_t
    {
        uint32_t rom_bank_offset = 0x4000;
        uint16_t ram_banks = 0;
        uint16_t ram_bank_offset = 0x0;
        uint32_t ram_bank_size = 0x0;
        uint16_t wram_offset = 0x1000;
        uint16_t wram_size = 0x2000;
        bool ram_enabled = false;
        bool rtc_enabled = false;
        bool rumble = false;
        uint16_t rom_bank_reg = 0x1;
        uint8_t mode_select = 0;
        uint8_t version = 1;
        std::array<uint8_t, 32768> wram;
    } m_state;
    // RAM is so big we want to deal with it dynamically
    std::array<uint8_t, 131072> m_ram;

    friend class Memory;
    void init();
};
} // namespace gbc
