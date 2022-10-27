
#include "gameboycore/mbc2.h"

#include "bitutil.h"

namespace gb
{
    namespace detail
    {
        MBC2::MBC2(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enable) :
            MBC(rom, size, rom_size, ram_size, cgb_enable)
        {
        }

        void MBC2::write(uint8_t value, uint16_t addr)
        {
            // MBC2 only uses the lower 4 bits
            MBC::write(value & 0x0F, addr);
        }

        void MBC2::control(uint8_t value, uint16_t addr)
        {
            if (addr <= 0x1FFF)
            {
                // least significant bit of upper byte in address must be zero 
                if (isClear(addr, 0x0100))
                {
                    xram_enable_ = ((value & 0x0F) == 0x0A);
                }
            }
            else if (addr >= 0x2000 && addr <= 0x3FFF)
            {
                // least significant bit of upper byte in address must be one
                if (isSet(addr, 0x0100))
                {
                    rom_bank_ = (value & 0x0F) - 1;
                }
            }
        }

        MBC2::~MBC2()
        {
        }
    }
}
