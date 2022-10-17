#include "Dma.hpp"
#include "Memory.hpp"
#include "Ppu.hpp"
#include "JoyPad.hpp"
#include "InesParser.hpp"
#include "Mapper.hpp"
#include "Bus.hpp"
Bus::Bus(Memory* memory, Ppu* ppu, JoyPad* joy_pad, Dma* dma, InesParser* ines_parser, Mapper* mapper){
    this->memory = memory;
    this->ppu    = ppu;
    this->joy_pad = joy_pad;
    this->dma = dma;
    this->ines_parser = ines_parser;
    this->mapper = mapper;
    assert(this->memory!=NULL);
    assert(this->ppu!=NULL);
    assert(this->joy_pad!=NULL);
    assert(this->dma!=NULL);
    assert(this->ines_parser!=NULL);
    assert(this->mapper!=NULL);
}

uint8_t Bus::Read8(uint16_t addr){
    PPU_REGISTER_KIND ppu_register_kind;
    if(0<=addr && addr<=0x07FF){
        return this->memory->memory[addr];
    }
    if(0x0800<=addr && addr<=0x1FFF){
        return this->memory->memory[addr-0x800];
    }  
    if(0x2000<=addr && addr<=0x2007){
        ppu_register_kind = (PPU_REGISTER_KIND)(addr-0x2000);
        return this->ppu->Read(ppu_register_kind);
    }
    if(0x2008<=addr && addr<=0x3FFF){
        addr = addr - 0x2008;
        ppu_register_kind = (PPU_REGISTER_KIND)(addr%8);
        return this->ppu->Read(ppu_register_kind);
    }
    if(addr==0x4016){
        return this->joy_pad->Read();
    }
    if(addr==0x4015){
        return 0;
    }
    if(0x4000<=addr && addr<=0x401F){
        return 0;
    }
    if(0x4020<=addr && addr<=0x5FFF){
        this->Error("Not implemented: addr(0x%04X~0x%04X) at Bus::Read8", 0x4020, 0x5FFF);
    }
    if(0x6000<=addr && addr<=0x7FFF){
        this->Error("Not implemented: addr(0x%04X~0x%04X) at Bus::Read8", 0x6000, 0x7FFF);
    }
    if(0x8000<=addr && addr<=0xBFFF){
        return this->memory->memory[addr];
    }
    if(0xC000<=addr && addr<=0xFFFF){
        if(this->ines_parser->GetPrgSize()==0x4000){
            return this->memory->memory[addr-0x4000];
        }
        return this->memory->memory[addr];
    }
    this->Error("Not implemented: addr = %04X at Bus::Read8", addr);
    return 0;
}

void Bus::Write8(uint16_t addr, uint8_t value){
    PPU_REGISTER_KIND ppu_register_kind;
    if(0<=addr && addr<=0x07FF){
        this->memory->memory[addr] = value;
        return;
    }
    if(0x0800<=addr && addr<=0x1FFF){
        this->memory->memory[addr] = value;
        return;
    }  
    if(0x2000<=addr && addr<=0x2007){
        ppu_register_kind = (PPU_REGISTER_KIND)(addr-0x2000);
        this->ppu->Write(ppu_register_kind, value);
        return;
    }
    if(0x2008<=addr && addr<=0x3FFF){
        this->Error("Not implemented: addr(0x%04X~0x%04X) at Bus::Write8", 0x2008, 0x3FFF);
    }
    if(addr==0x4014){
        this->dma->Write(value);
        return;
    }
    if(addr>=0x4000&&addr<=0x4013){
        return;
    }
    if(addr==0x4015){
        return;
    }
    if(addr==0x4017){
        return;
    }
    if(addr==0x4016){
        this->joy_pad->Write(value);
        return;
    }
    if(addr==0x4017){
        return;
    }
    if(0x4000<=addr && addr<=0x401F){
        this->Error("Not implemented: addr = %04X at Bus::Write8", addr);
    }
    if(0x4020<=addr && addr<=0x5FFF){
        this->Error("Not implemented: addr(0x%04X~0x%04X) at Bus::Write8", 0x4020, 0x5FFF);
    }
    if(0x6000<=addr && addr<=0x7FFF){
        this->memory->memory[addr] = value;
        return;
    }
    if(addr>=0x8000&&addr<=0xFFFF){
        printf("MAYBE BAD MAPPER?\n");
        this->mapper->Write(value);
        return;
    }
    fprintf(stderr, "addr = %08X, bank = %d\n", addr, value);
    this->Error("Not implemented: addr = %04X at Bus::Write8", addr);
}
