#include "InesParser.hpp"
#include "Memory.hpp"

Memory::Memory(InesParser* ines_parser){
    this->ines_parser = ines_parser;
    int memory_size = 0x0000FFFF+1;
    this->LoadPrg();
}

void Memory::Dump(int addr, int cnt){
    for(int i=0; i<cnt; i++){
        fprintf(stderr, "%02X ", this->memory[addr+i]);
        if((i+1)%16==0){
            cout << endl;
        }
    }
}

void Memory::LoadPrg(){
    int prg_size = this->ines_parser->GetPrgSize();
    char* buff = this->ines_parser->GetPrgRom();
    memcpy(this->memory+0x8000, buff, prg_size);
    if(prg_size==0x4000){
        memcpy(this->memory+0xC000, buff, prg_size);
    }
}