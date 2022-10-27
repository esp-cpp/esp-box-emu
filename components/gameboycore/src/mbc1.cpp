#include "gameboycore/mbc1.h"

namespace gb
{
    namespace detail
    {
        MBC1::MBC1(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enabled) :
            MBC(rom, size, rom_size, ram_size, cgb_enabled),
            rom_bank_lower_bits_(0),
            rom_bank_upper_bits_(0),
            mode_(MemoryMode::ROM)
        {
        }

        void MBC1::control(uint8_t value, uint16_t addr)
        {
            if (addr <= 0x1FFF)
            {
                xram_enable_ = ((value & 0x0F) == 0x0A);
            }
            else if (addr >= 0x2000 && addr <= 0x3FFF)
            {
                rom_bank_lower_bits_ = value & 0x1F;

                if (mode_ == MemoryMode::ROM)
                {
                    selectRomBank(rom_bank_lower_bits_, rom_bank_upper_bits_);
                }
                else
                {
                    rom_bank_upper_bits_ = 0;
                    selectRomBank(rom_bank_lower_bits_, rom_bank_upper_bits_);
                }
            }
            else if (addr >= 0x4000 && addr <= 0x5FFF)
            {
                if (mode_ == MemoryMode::ROM)
                {
                    rom_bank_upper_bits_ = value & 0x03;
                    selectRomBank(rom_bank_lower_bits_, rom_bank_upper_bits_);
                }
                else
                {
                    selectRamBank(value & 0x3);
                }
            }
            else if (addr >= 0x6000 && addr <= 0x7FFF)
            {
                mode_ = static_cast<MemoryMode>(value & 0x01);

                if (mode_ == MemoryMode::RAM)
                {
                    rom_bank_upper_bits_ = 0;
                    selectRomBank(rom_bank_lower_bits_, rom_bank_upper_bits_);
                }
            }
        }

        void MBC1::selectRomBank(uint8_t lo, uint8_t hi)
        {
            auto bank_number = ((hi << 5) | lo);

            // potentially remap the rom bank number
            switch (bank_number)
            {
            case 0x00:
            case 0x20:
            case 0x40:
            case 0x60:
                bank_number++;
                break;
            default:
                // ...
                break;
            }

            rom_bank_ = bank_number - 1;
        }

        void MBC1::selectRamBank(uint8_t ram_bank_number)
        {
            ram_bank_ = ram_bank_number;
        }
    }
}