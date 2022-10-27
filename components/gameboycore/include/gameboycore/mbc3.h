/**
    \file mbc2.h
    \brief Memory Bank Controller 3
    \author Natesh Narain
    \date Nov 20 2016
*/

#ifndef GAMEBOYCORE_MBC3_H
#define GAMEBOYCORE_MBC3_H

#include "gameboycore/mbc.h"
#include "gameboycore/detail/rtc/rtc.h"
#include <array>

namespace gb
{
    namespace detail
    {
        /**
            \class MBC3
            \brief Memory Bank Controller 3
            \ingroup MBC
        */
        class MBC3 : public MBC
        {
        public:
            MBC3(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enable);
            ~MBC3();

            virtual uint8_t read(uint16_t addr) const;
			
			void setTimeProvider(TimeProvider provider);

        protected:
            virtual void control(uint8_t value, uint16_t addr);

        private:
            RTC rtc_;
			uint8_t latch_ctl_;
        };
    }
}

#endif