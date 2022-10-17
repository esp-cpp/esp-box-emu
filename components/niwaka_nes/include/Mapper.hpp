#pragma once
#include "common.hpp"
using namespace std;
class InesParser;

class Mapper:public Object{
    protected:
        uint8_t* chr_rom = NULL;
        int chr_rom_size;
        uint8_t bank_select_register = 0;
    public: 
        Mapper(InesParser* ines_parser);
        virtual void Write(uint8_t data)    = 0;
        virtual uint8_t* ReadChrRom(uint16_t addr) = 0;
};