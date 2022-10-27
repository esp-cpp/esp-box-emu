//
// Created by Ethan Williams on 2019-02-04.
//

#ifndef NESEMU_PPUMEMORY_H
#define NESEMU_PPUMEMORY_H

#include "utils.h"
#include "Gamepak.h"
#include <cstdint>

#define VRAM_SIZE 2*KILO
#define OAM_ENTRIES 64

struct OAM_entry {
		uint8_t y_coordinate;
		uint8_t tile_number;
		union {
			uint8_t data;
			RegBit<0,2,uint8_t> palette;
			RegBit<5,1,uint8_t> priority;
			RegBit<6,1,uint8_t> flip_H;
			RegBit<7,1,uint8_t> flip_V;
		} attribute;
		uint8_t x_coordinate;
};


class PPUmemory {
private:
		Gamepak * gamepak;
		uint8_t nametable_vram[VRAM_SIZE];
		union {
				OAM_entry entries[OAM_ENTRIES];
				uint8_t data[OAM_ENTRIES * sizeof(OAM_entry)];
		} OAM;
		uint8_t palette_RAM[32];
		uint8_t read_buffer;

public:
		explicit PPUmemory(Gamepak * gamepak);
		~PPUmemory();

		uint8_t buffered_read_byte(uint16_t address);
		uint8_t direct_read_byte(uint16_t address);
		void write_byte(uint16_t address, uint8_t value);
		uint8_t read_byte_OAM(uint8_t address);
		void write_byte_OAM(uint8_t address, uint8_t value);
		OAM_entry * read_entry_OAM(uint8_t index);
};


#endif //NESEMU_PPUMEMORY_H
