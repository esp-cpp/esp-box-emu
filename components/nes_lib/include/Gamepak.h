//
// Created by Ethan Williams on 2019-01-28.
//

#ifndef NESEMU_GAMEPAK_H
#define NESEMU_GAMEPAK_H

#include <string>
#include <vector>
#include "utils.h"

struct iNES_headers {
    char magic_string[4]; // bytes 0-3
		uint8_t PRG_ROM_size_lsb; // byte 4
		uint8_t CHR_ROM_size_lsb; // byte 5

		union {
				uint8_t data;
				RegBit<0,1,uint8_t> mirroring;
				RegBit<1,1,uint8_t> battery;
				RegBit<2,1,uint8_t> trainer;
				RegBit<3,1,uint8_t> fourscreen;
				RegBit<4,4,uint8_t> mapper_lsb;
		} byte6;

		union {
				uint8_t data;
				RegBit<0,2,uint8_t> console_type;
				RegBit<2,2,uint8_t> iNES2_id;
				RegBit<4,4,uint8_t> mapper_msb;
		} byte7;

		union {
				uint8_t data;
				RegBit<0,8,uint8_t> iNES1_PRG_RAM_size;     RegBit<0,4,uint8_t> iNES2_mapper_xsb;
				                                            RegBit<4,4,uint8_t> iNES2_submapper;
		} byte8;

		// Bytes 9-15 iNES 2.0 ONLY
		union {
				uint8_t data;
				RegBit<0,4,uint8_t> iNES2_PRG_ROM_size_msb;
				RegBit<4,4,uint8_t> iNES2_CHR_ROM_size_msb;
		} byte9;

		union {
				uint8_t data;
				RegBit<0,4,uint8_t> iNES2_PRG_RAM_size;
				RegBit<4,4,uint8_t> iNES2_PRG_NVRAM_size;
		} byte10;

		union {
				uint8_t data;
				RegBit<0,4,uint8_t> iNES2_CHR_RAM_size;
				RegBit<4,4,uint8_t> iNES2_CHR_NVRAM_size;
		} byte11;

		/* Ignore bytes 12-15 */
};

union MMC1_regfile {
	uint16_t data;
	RegBit<0,8,uint16_t> ctrlreg;
	RegBit<0,2,uint16_t> mirroring;
	RegBit<2,2,uint16_t> PRGmode;
	RegBit<4,1,uint16_t> CHRmode;
	RegBit<8,8,uint16_t> shift;
	RegBit<8,1,uint16_t> shift_input_byte;
};

class Gamepak {
private:
    std::string filename;
    std::vector<uint8_t> rom_data;
    uint8_t *trainer;
    size_t PRG_rom_data;
    size_t PRG_rom_bank1;
    size_t PRG_rom_bank2;
    size_t CHR_rom_data;
    size_t PRG_blocks;
    size_t PRG_size;
    size_t CHR_size;
    size_t debugfilesize;
    iNES_headers * headers;
    uint16_t mapper;
    std::string nametable_mirroring_type;
    MMC1_regfile MMC1reg;
    size_t current_chr_bank1;
    size_t current_chr_bank2;
    size_t shift_counter;

    std::vector<uint8_t> PRG_ram;
    std::vector<uint8_t> CHR_ram;

	int verifyHeaders();
	void initMemory();

public:
    explicit Gamepak(std::string filename);
    ~Gamepak();
    int initialize();

    void write_PRG(uint16_t address, uint8_t value);
    uint8_t read_PRG(uint16_t address);

		void write_CHR(uint16_t address, uint8_t value);
		uint8_t read_CHR(uint16_t address);
		uint16_t translate_nametable_address(uint16_t address);

		void debug_writeback();



};


#endif //NESEMU_GAMEPAK_H
