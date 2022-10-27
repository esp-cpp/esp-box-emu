/**
 * \file mbc1.h
 * \author Natesh Narain <nnaraindev@gmail.com>
 * \brief Memory Back Controller 1
 * \date Oct 11 2016
*/
#ifndef GAMEBOYCORE_MBC1_H
#define GAMEBOYCORE_MBC1_H

#include "gameboycore/mbc.h"

#include <vector>

namespace gb
{
    namespace detail
    {
        /**
            \class MBC1
            \brief Memory Bank Controller 1
            \ingroup MBC
        */
        class MBC1 : public MBC
        {
        private:

            //! RAM or ROM bank switching mode
            enum class MemoryMode
            {
                ROM = 0, RAM = 1 ///< determines how address range $4000 - $5000 is used
            };

        public:
            MBC1(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enabled);

        protected:
            virtual void control(uint8_t value, uint16_t addr);

        private:
            void selectRomBank(uint8_t lo, uint8_t hi);
            void selectRamBank(uint8_t ram_bank_number);

            uint8_t rom_bank_lower_bits_; // bit 0 - 4
            uint8_t rom_bank_upper_bits_; // bit 5 and 6

            MemoryMode mode_;
        };
    }
}

#endif
