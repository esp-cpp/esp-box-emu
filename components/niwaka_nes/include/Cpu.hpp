#pragma once
#include "common.hpp"
#include "Bus.hpp"

enum REGISTER_KIND {A_KIND, X_KIND, Y_KIND, S_KIND, REGISTER_KIND_CNT};

class Cpu;
class InstructionBase;
class Gui;
class Memory;
class Ppu;
class Dma;
class Bus;
class InterruptManager;

#define INSTRUCTION_SIZE 256
class Cpu:public Object{
    private:
        int now_cycle = 0;
        int instruction_size = INSTRUCTION_SIZE;
        InstructionBase* instructions[INSTRUCTION_SIZE];
        uint8_t gprs[REGISTER_KIND_CNT];
        uint16_t pc;
        union{
            uint8_t raw;
            struct{
                unsigned C:1;
                unsigned Z:1;
                unsigned I:1;
                unsigned D:1;
                unsigned B:1;
                unsigned R:1;
                unsigned V:1;
                unsigned N:1;
            }flgs;
        }P;
        Bus* bus;
        vector<string> instruction_log;
    public:
        Cpu(Bus* bus);
        int Execute();
        void SetI();
        void SetN();
        void SetZ();
        void SetC();
        void SetV();
        void SetD();
        void SetB();
        void ClearI();
        void ClearN();
        void ClearZ();
        void ClearC();
        void ClearV();
        void ClearD();
        void AddPc(uint16_t value);
        void SetPc(uint16_t value);
        void Set8(REGISTER_KIND register_kind, uint8_t value);
        void SetP(uint8_t value);
        void ShowSelf();
        uint16_t GetPc();
        uint8_t GetGprValue(REGISTER_KIND register_kind);
        uint8_t GetP();
        uint8_t Read8(uint16_t addr);
        uint16_t Read16(uint16_t addr);
        template<typename TYPE>void Write(uint16_t addr, TYPE value){
            uint8_t* p = (uint8_t*)(&value);
            for(int i=0; i<sizeof(value); i++){
                this->bus->Write8(addr+i, p[i]);
            }
        }
        void UpdateNflg(uint8_t value);
        void UpdateZflg(uint8_t value);
        bool IsNflg();
        bool IsZflg();
        bool IsCflg();
        bool IsVflg();
        uint8_t GetCFLg();
        void Push16(uint16_t data);
        void Push8(uint8_t data);
        template<typename type>void Push(type data){
            this->Error("Not implemented: Cpu::Push");
        } 
        uint16_t Pop16();
        uint8_t Pop8();
        void HandleNmi(InterruptManager* interrupt_manager);
        bool CmpLastInstructionName(string name);
        void Reset();
};
