#ifndef GAMEBOYCORE_MBC5_H
#define GAMEBOYCORE_MBC5_H

#include "gameboycore/mbc.h"

namespace gb
{
    namespace detail
    {
        class MBC5 : public MBC
        {
        public:

            MBC5(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enable);
            ~MBC5();

            void control(uint8_t value, uint16_t addr);

        private:
            void selectRomBank(uint8_t lo, uint8_t hi);

            uint8_t rom_bank_lower_bits_;
            uint8_t rom_bank_upper_bit_;
        };
    }
}

#endif // GAMEBOYCORE_MBC5_H
