#pragma once
#include "common.hpp"
using namespace std;

class Memory;
class Ppu;

class Dma:public Object{
    private:
        uint16_t start_addr;
        bool running = false;
        Memory* mem;
    public:
        Dma(Memory* mem);
        void Write(uint8_t addr);
        bool IsRunning();
        void Execute(Ppu* ppu);
};