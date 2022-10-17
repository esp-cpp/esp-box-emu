#pragma once
#include "common.hpp"

class Gui;
class Ppu;
class Dma;
class Bus;
class Memory;
class InesParser;
class InterruptManager;
class JoyPad;
class Mapper;
class Cpu;

class Emulator:public Object{
    private:
        unique_ptr<JoyPad> joy_pad;
        unique_ptr<InesParser> ines_parser;
        unique_ptr<Mapper> mapper;
        unique_ptr<InterruptManager> interrupt_manager;
        unique_ptr<Memory> memory;
        unique_ptr<Dma> dma;
        unique_ptr<Gui> gui;
        unique_ptr<Ppu> ppu;
        unique_ptr<Bus> bus;
        unique_ptr<Cpu> cpu;
        int now_cycle = 0;
    public:
        Emulator(const char* filename);
        void Execute();
};
