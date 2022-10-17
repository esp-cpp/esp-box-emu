#include "Mapper.hpp"
#include "Mapper3.hpp"

Mapper3::Mapper3(InesParser* ines_parser):Mapper(ines_parser){

}

void Mapper3::Write(uint8_t data){
    this->bank_select_register = data&0x03;
}

uint8_t* Mapper3::ReadChrRom(uint16_t addr){
    return &this->chr_rom[addr+this->bank_select_register*0x2000];
}