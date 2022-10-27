/**
    \file mbc2.h
    \brief Memory Bank Controller 2
    \author Natesh Narain
    \date Nov 20 2016
*/

#ifndef GAMEBOYCORE_MBC2_H
#define GAMEBOYCORE_MBC2_H

#include "gameboycore/mbc.h"

namespace gb
{
    namespace detail
    {
        /**
            \class MBC2
            \brief Memory Bank Controller 2
            \ingroup MBC
        */
        class MBC2 : public MBC
        {
        public:

            MBC2(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enable);
            ~MBC2();

            virtual void write(uint8_t value, uint16_t addr);

        protected:
            virtual void control(uint8_t value, uint16_t addr);

        private:
        };
    }
}

#endif // !GAMEBOYCORE_MBC2_H

