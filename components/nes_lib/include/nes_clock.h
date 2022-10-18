/*
 * nes_clock.h will define master clock, cpu clock, and ppu clock
 * This will probably be modified as time goes on
 */

#ifndef NESEMU_NES_CLOCK_H
#define NESEMU_NES_CLOCK_H

#include <chrono>

// We will set the master clock to be that of the PPU Clock since it is faster
#define MASTER_CLOCK 21477271ll

typedef std::chrono::duration<uint64_t, std::ratio<1,(MASTER_CLOCK/4)>> nes_master_clock_t;
typedef std::chrono::duration<uint64_t, std::ratio<1,(MASTER_CLOCK/4)>> nes_ppu_clock_t;
typedef std::chrono::duration<uint64_t, std::ratio<1, (MASTER_CLOCK/12)>> nes_cpu_clock_t;



#endif //NESEMU_NES_CLOCK_H
