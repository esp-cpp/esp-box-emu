#ifndef GAMEBOYCORE_INTERRUPT_PROVIDER_H
#define GAMEBOYCORE_INTERRUPT_PROVIDER_H

#include "gameboycore/mmu.h"
#include "gameboycore/memorymap.h"

#include <cstdint>

namespace gb
{
    /**
        \class InterruptProvider
        \brief Used to set interrupt flag register for a single interrupt
    */
    class InterruptProvider
    {
    public:
        enum class Interrupt
        {
            VBLANK  = (1 << 0),
            LCDSTAT = (1 << 1),
            TIMER   = (1 << 2),
            SERIAL  = (1 << 3),
            JOYPAD  = (1 << 4)
        };

    public:
        InterruptProvider(MMU& mmu, Interrupt interrupt) :
            flags_(mmu.get(memorymap::INTERRUPT_FLAG)),
            interrupt_(interrupt)
        {
        }

        /**
            Set the interrupt
        */
        void set()
        {
            flags_ |= static_cast<uint8_t>(interrupt_);
        }

        ~InterruptProvider()
        {
        }

    private:
        uint8_t& flags_;
        Interrupt interrupt_;
    };

}

#endif
