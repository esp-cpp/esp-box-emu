/**
 * \file timer.h
 * \author Natesh Narain <nnaraindev@gmail.com>
 * \date   Oct 15 2016
*/

#ifndef GAMEBOY_TIMER_H
#define GAMEBOY_TIMER_H

#include "gameboycore/mmu.h"
#include "gameboycore/memorymap.h"
#include "gameboycore/interrupt_provider.h"

#include <cstdint>
#include <functional>

namespace gb
{
    /**
        \brief Opcode accurate timer
    */
    class Timer
    {
    public:
        Timer(MMU& mmu);
        ~Timer();

        void update(const uint8_t cycles);

    private:
        void tick();

        uint8_t& controller_; // TAC
        uint8_t& counter_;    // TIMA
        uint8_t& modulo_;     // TMA
        uint8_t& divider_;    // DIV

        int t_clock_;
        int base_clock_;
        int div_clock_;

        InterruptProvider timer_interrupt_;
    };
}

#endif // GAMEBOYCORE_TIMER_H
