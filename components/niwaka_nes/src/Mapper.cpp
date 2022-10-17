#include "InesParser.hpp"
#include "Mapper.hpp"

Mapper::Mapper(InesParser* ines_parser){
    assert(ines_parser!=NULL);
    this->chr_rom      = (uint8_t*)ines_parser->GetChrRom();
    assert(this->chr_rom!=NULL);
    this->chr_rom_size = ines_parser->GetChrSize();
}
