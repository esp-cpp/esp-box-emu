#include "gameboycore/mbc5.h"

namespace gb
{
    namespace detail
    {
        MBC5::MBC5(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enable) : 
            MBC(rom, size, rom_size, ram_size, cgb_enable),
            rom_bank_lower_bits_(0),
            rom_bank_upper_bit_(0)
        {
        }

        void MBC5::control(uint8_t value, uint16_t addr)
        {
            if (addr <= 0x1FFF)
            {
                // enable / disable external ram
                xram_enable_ = ((value & 0x0F) == 0x0A);
            }
            else if (addr >= 0x2000 && addr <= 0x2FFF)
            {
                // lower 8 bits of rom bank number
                rom_bank_lower_bits_ = value;
                selectRomBank(rom_bank_lower_bits_, rom_bank_upper_bit_);
            }
            else if (addr >= 0x3000 && addr <= 0x3FFF)
            {
                // 9th bit of rom bank number
                rom_bank_upper_bit_ = value;
                selectRomBank(rom_bank_lower_bits_, rom_bank_upper_bit_);
            }
            else if (addr >= 0x4000 && addr <= 0x5FFF)
            {
                // ram bank number
                ram_bank_ = value & 0x0F;
            }
        }

        void MBC5::selectRomBank(uint8_t lo, uint8_t hi)
        {
            rom_bank_ = ((hi & 0x0001) << 8) | (lo & 0xFFFF);

            if (rom_bank_ > 0) rom_bank_ -= 1;
        }

        MBC5::~MBC5()
        {
        }
    }
}