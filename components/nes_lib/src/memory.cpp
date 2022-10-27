#include "nes_lib/memory.h"
#include <cstdio>
#include <iostream>

NesCPUMemory::NesCPUMemory(PPU *ppu, Gamepak *gamepak, InputDevice *joypad1, InputDevice *joypad2) {
	this->ppu = ppu;
	this->gamepak = gamepak;
	this->joypad1 = joypad1;
	this->joypad2 = joypad2;
}

NesCPUMemory::~NesCPUMemory() = default;

uint8_t NesCPUMemory::read_byte(uint16_t address) {
	if (address < 0x2000) { // CPU RAM, mirrored 4 times
		return this->cpu_ram[address % 0x800];
	}
	else if (address >= 0x2000 && address < 0x4000) { // PPU registers, mirrored every 8 bytes
		return ppu->read_register((uint8_t)(address % 0x8));
	}
    else if (address >= 0x4000 && address < 0x4018) { // APU/IO registers, not mirrored
        if (address == 0x4016) {
            return joypad1->readController();
        }
        else if (address == 0x4017) {
            return joypad2->readController();
        }
        else {
            return APU_IO_register_file.data[address % 0x18];
        }

	}
	else if (address >= 0x4018 && address < 0x4020) { // Disabled APU/IO functionality
		return 0;
	}
	else if (address >= 0x4020 && address <= 0xFFFF) { // GamePak memory
		return gamepak->read_PRG(address);
	}
	else return 0;
}

nes_cpu_clock_t NesCPUMemory::write_byte(uint16_t address, uint8_t value) {

    //if (address > 0x7FFF) { // non-writable Gamepak ROM
    //    return;
   // }
   nes_cpu_clock_t cycles = nes_cpu_clock_t(0);
    if (address < 0x2000) { // CPU RAM, mirrored 4 times
	    cpu_ram[address % 0x800] = value;
    }
    else if (address >= 0x2000 && address < 0x4000) { // PPU registers, mirrored every 8 bytes
    	ppu->write_register(address, value);
    }
    else if (address == 0x4014) { // OAM DMA
    	ppu->OAM_DMA(&cpu_ram[(uint16_t)value << 8]);
    	cycles = nes_cpu_clock_t(512);
    }
    else if (address >= 0x4000 && address < 0x4018) { // APU/IO registers, not mirrored
		if (address == 0x4016) {
            joypad1->writeController(value);
            joypad2->writeController(value);
		}
		else {
            APU_IO_register_file.data[address % 0x18] = value;
        }
    }
    else if (address >= 0x4018 && address < 0x4020) { // Disabled APU/IO functionality
			return nes_cpu_clock_t(0);
    }
    else if (address >= 0x4020 && address <= 0xFFFF) { // GamePak memory
	    gamepak->write_PRG(address, value);
    }
    return cycles;
}

uint16_t NesCPUMemory::read_word(uint16_t address) {
    return (read_byte(address) + (uint16_t(read_byte(address+uint16_t(1))) << 8));
}

uint16_t NesCPUMemory::read_word_page_bug(uint16_t address) {
    uint16_t first = read_byte(address);
    uint16_t high;
    if ((address&0xFF) == 0xFF) {
       high = (uint16_t)(address & 0xFF00);
    }
    else {
        high = (uint16_t)(address + 1);
    }
    uint16_t second = read_byte(high);
    return (first | (second << 8));
}

void NesCPUMemory::write_word(uint16_t address, uint16_t value) {
    write_byte(address, value & 0xff); //TODO: Make sure this is okay
    write_byte(address+1, (value >> 8));
}

void NesCPUMemory::stack_write_word(uint16_t address, uint16_t value) {
    write_byte(address, (value >> 8));
    write_byte(address-1, value & 0xff);
}

void NesCPUMemory::map_memory(uint16_t address, uint8_t *data, size_t size) {
    for (int i = 0; i < size; ++i) {
        write_byte(address+i, data[i]);
    }
}

//void NesCPUMemory::printTest() {
//    std::cout << std::hex << +this->ram[0x6000] << std::endl;
//}
