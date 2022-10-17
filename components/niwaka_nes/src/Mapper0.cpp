#include "Mapper.hpp"
#include "Mapper0.hpp"

Mapper0::Mapper0(InesParser* ines_parser):Mapper(ines_parser){
    
}

void Mapper0::Write(uint8_t data){
    this->Error("Not implemented: Mapper0::Write");
}

uint8_t* Mapper0::ReadChrRom(uint16_t addr){
    return &this->chr_rom[addr];
}