#pragma once
#include "common.hpp"
using namespace std;

class Memory;
class Ppu;
class JoyPad;
class Dma;
class InesParser;
class Mapper;

class Bus:public Object{
    private:
        Ppu* ppu = NULL;
        Memory* memory = NULL;
        JoyPad* joy_pad = NULL;
        Dma* dma = NULL;
        InesParser* ines_parser = NULL;
        Mapper* mapper = NULL;
    public: 
        Bus(Memory* memory, Ppu* ppu, JoyPad* joy_pad, Dma* dma, InesParser* ines_parser, Mapper* mapper);
        uint8_t Read8(uint16_t addr);
        void Write8(uint16_t addr, uint8_t value);
};