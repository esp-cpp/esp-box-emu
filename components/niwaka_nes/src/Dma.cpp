#include "Memory.hpp"
#include "Ppu.hpp"
#include "Dma.hpp"

Dma::Dma(Memory* mem){
    this->running = false;
    this->mem = mem;
    assert(this->mem!=NULL);
}

void Dma::Write(uint8_t addr){
    this->start_addr = ((uint16_t)addr)<<8;
    this->running    = true;
}

bool Dma::IsRunning(){
    return this->running;
}

void Dma::Execute(Ppu* ppu){
    assert(ppu!=NULL);
    for(int i=0; i<256; i++){
        ppu->WriteSprite(i, this->mem->memory[this->start_addr+i]);
    }
    this->running = false;
}