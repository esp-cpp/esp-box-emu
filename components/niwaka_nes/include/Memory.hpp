#pragma once
#include "common.hpp"
using namespace std;

class InesParser;

class Memory:public Object{
    private:
        InesParser* ines_parser;
        int memory_size;
    public:
        uint8_t memory[0x00010000];
        Memory(InesParser* ines_parser);
        void Dump(int addr, int cnt);//Debug
        void LoadPrg();
};