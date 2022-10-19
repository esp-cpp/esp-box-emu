//
// Created by Ethan Williams on 2019-01-28.
//

#include "../include/Gamepak.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <utility>

Gamepak::Gamepak(std::string filename) {
    this->filename = std::move(filename);
    trainer = nullptr;
    headers = nullptr;
    mapper = 0;
    PRG_ram.assign(8*KILO, 0);
    CHR_ram.assign(8*KILO, 0);
}

Gamepak::~Gamepak() {
}

int Gamepak::initialize(uint8_t *raw_romdata) {
    raw_rom_data = raw_romdata;

    if (verifyHeaders()) {
        return EXIT_FAILURE;
    }
    initMemory();

    std::cout << "ROM initialized." << std::endl;
    return EXIT_SUCCESS;
}

int Gamepak::initialize() {
    std::ifstream romfile(filename, std::ios::binary | std::ios::ate); //open file at end
    if (!romfile.is_open()) {
        std::cerr << "Error: ROM file does not exist" << std::endl;
        return EXIT_FAILURE;
    }

    std::streampos filesize = romfile.tellg(); // get size from current file pointer location;
    this->debugfilesize = (size_t)filesize;
    romfile.seekg(0, std::ios::beg); //reset file pointer to beginning;

    //rom_data = new char[filesize];
    rom_data.assign(filesize, 0);
    romfile.read((char*)&rom_data[0], filesize);
    romfile.close();

    return initialize(rom_data.data());
}

int Gamepak::verifyHeaders() {
    headers = (iNES_headers *) raw_rom_data;
    if (strncmp(headers->magic_string, "NES\x1A", 4) != 0) {
        std::cerr << "Error: ROM not in iNES format" << std::endl;
        return EXIT_FAILURE;
    }

    if (headers->byte7.iNES2_id == 0b10) {
    	std::cout << "iNES 2.0 format detected" << std::endl;
    	mapper = headers->byte8.iNES2_mapper_xsb << 8 | headers->byte7.mapper_msb << 4 | headers->byte6.mapper_lsb;
        PRG_blocks = headers->byte9.iNES2_PRG_ROM_size_msb << 8 | headers->PRG_ROM_size_lsb;
        PRG_size = PRG_blocks * 16u * KILO;
        CHR_size = (headers->byte9.iNES2_CHR_ROM_size_msb << 8 | headers->CHR_ROM_size_lsb) * 8u * KILO;
        //delete [] CHR_ram;
        CHR_ram.assign(64 << (headers->byte11.iNES2_CHR_RAM_size), 0);
        //CHR_ram = new uint8_t[(headers->byte11.iNES2_CHR_RAM_size)*KILO];

    } else {
        std::cout << "iNES 1.x format detected" << std::endl;
        mapper = headers->byte7.mapper_msb << 4 | headers->byte6.mapper_lsb;
        PRG_blocks = headers->PRG_ROM_size_lsb;
        PRG_size = PRG_blocks * 16u * KILO;
        CHR_size = headers->CHR_ROM_size_lsb * 8u * KILO;
    }

    if (mapper > 2) { // Any mapper other than 0 or 1
        std::cerr << "Invalid or unsupported mapper: " << mapper << std::endl;
        return EXIT_FAILURE;
    } else std::cout << "Mapper " << mapper << " detected" << std::endl;


    /* Handle flags (in bit order) */
    nametable_mirroring_type = headers->byte6.mirroring | 1 ? "vertical" : "horizontal";
    std::cout << "Mirroring type: " << nametable_mirroring_type << std::endl;

    if (headers->byte6.battery) { //battery RAM
        std::cout << "Battery RAM present" << std::endl;
        /* Currently we ignore this flag and assume 8k volatile RAM present */
        //TODO: Implement some kind of way to save data
    }

    if (headers->byte6.trainer) {
	      std::cout << "Trainer present" << std::endl;
		    trainer = (uint8_t *)(raw_rom_data+16);
		    memcpy(&PRG_ram[0x1000],trainer,512);
		    PRG_rom_data = 528;
    } else {
    	PRG_rom_data = 16;
    }
    CHR_rom_data = PRG_rom_data + PRG_size;

    std::cout << "PRG Blocks = " << PRG_blocks << std::endl;

    return EXIT_SUCCESS;
}

void Gamepak::initMemory() {
    if (mapper == 0) {
        PRG_rom_bank1 = PRG_rom_data;
        if (PRG_size > 16*KILO) {
            PRG_rom_bank2 = PRG_rom_data + 16*KILO;
        } else PRG_rom_bank2 = PRG_rom_bank1;
    } else if (mapper == 1) {
        shift_counter = 0;
        PRG_rom_bank1 = PRG_rom_data;
        PRG_rom_bank2 = PRG_rom_data + (PRG_blocks-1)*16u*KILO;
        MMC1reg.ctrlreg = 0x0C;
        MMC1reg.shift = 0x00;
        current_chr_bank1 = 0;
        current_chr_bank2 = 0;
    }
    else if (mapper == 2) {
        PRG_rom_bank1 = PRG_rom_data;
        PRG_rom_bank2 = PRG_rom_data + (PRG_blocks-1)*16u*KILO;
    }
    std::cout << "PRG Bank 1 = " << PRG_rom_bank1 << std::endl;
    std::cout << "PRG Bank 2 = " << PRG_rom_bank2 << std::endl;
    std::cout << "CHR = " << CHR_rom_data << std::endl;
    std::cout << "Size of container = " << rom_data.size() << std::endl;
}


void Gamepak::write_PRG(uint16_t address, uint8_t value) {
    if (address < 0x6000) return;
    else if (address < 0x8000) { // RAM, same for mapper 0 and 1
        PRG_ram.at((uint16_t)(address % 0x2000)) = value;
    } else if (mapper == 1) { // writing to control register
        if (value >> 7) { //reset
            shift_counter = 0;
            MMC1reg.shift = 0x00;
            MMC1reg.ctrlreg = MMC1reg.ctrlreg | 0x0C;
            PRG_rom_bank2 = PRG_rom_data + (PRG_blocks-1)*16u*KILO;
        }
        else {
            shift_counter++;
            MMC1reg.shift =  ((MMC1reg.shift >> 1)|((value&0x1) << 4));

            if (shift_counter == 5) {
                switch (address&0xE000) {
                    case 0x8000: {
                        MMC1reg.mirroring = (MMC1reg.shift & 0x3);
                        MMC1reg.PRGmode = (MMC1reg.shift & 0xC) >> 2;
                        MMC1reg.CHRmode = (MMC1reg.shift & 0x10) >> 4;
                        break;
                    }
                    case 0xA000: {
                        if (MMC1reg.CHRmode == 0) {
                            current_chr_bank1 = (MMC1reg.shift & 0x1E);
                        }
                        else {
                            current_chr_bank1 = MMC1reg.shift;
                        }
                        break;
                    }
                    case 0xC000: {
                        current_chr_bank2 = MMC1reg.shift;
                        break;
                    }
                    case 0xE000: {
                        uint32_t prg_bank_modified = (MMC1reg.shift&0xE);
                        uint32_t prg_bank = (MMC1reg.shift&0xF);
                        if (prg_bank_modified > this->PRG_blocks || prg_bank > this->PRG_blocks) {
                            std::cerr << "Attempting to swtich to bank " << prg_bank << " which is out of bounds." << std::endl;
                        }
                        else if ((MMC1reg.PRGmode == 0) || (MMC1reg.PRGmode == 1)) {
                            PRG_rom_bank1 = PRG_rom_data+(prg_bank_modified*16u*KILO);
                            PRG_rom_bank2 = PRG_rom_data+((prg_bank_modified+1u)*16u*KILO);
                        }
                        else if (MMC1reg.PRGmode == 2) {
                            PRG_rom_bank1 = PRG_rom_data;
                            PRG_rom_bank2 = PRG_rom_data+(prg_bank*16u*KILO);
                        }
                        else if (MMC1reg.PRGmode == 3) {
                            PRG_rom_bank1 = PRG_rom_data+(prg_bank*16u*KILO);
                            PRG_rom_bank2 = PRG_rom_data+(PRG_blocks-1)*16u*KILO;
                        }
                        break;
                    }
                    default: {
                    }
                }
                MMC1reg.shift = 0;
                shift_counter = 0;
            }
        }
    }
    else if (mapper == 2) {
        PRG_rom_bank1 = PRG_rom_data + ((((size_t)value&0x7)*16u*KILO));
    }
}

uint8_t Gamepak::read_PRG(uint16_t address) {
    if (address < 0x6000) {
        std::cerr << "WARNING: TRYING TO READ FROM UNMAPPED MEMORY: 0x " << std::hex << +address << std::endl;
        return 0;
    }
    else if (address < 0x8000) { // RAM, same between mapper 0 and 1
        return PRG_ram.at(uint16_t(address % 0x2000));
    }
    else if (address < 0xC000) {
        return raw_rom_data[PRG_rom_bank1+size_t((address % 0x4000))];
    } else return raw_rom_data[PRG_rom_bank2+size_t((address % 0x4000))];
}

void Gamepak::write_CHR(uint16_t address, uint8_t value) {
    if (address >= 0x2000) return;
    if (CHR_size == 0) {
        if (mapper == 0 || mapper == 2) {
            CHR_ram.at(address) = value;
        }
        else if (mapper == 1) {
            if (MMC1reg.CHRmode == 0) {
                CHR_ram.at((KILO * 4u * current_chr_bank1) + address) = value;
            }
            else {
                if (address <= 0x0FFF) {
                    CHR_ram.at((KILO * 4u * current_chr_bank1) + address) = value;
                } else if (address <= 0x1FFF) {
                    CHR_ram.at((KILO * 4u * current_chr_bank2) + (address-0x1000)) = value;
                }
            }
        }
    }
}

uint8_t Gamepak::read_CHR(uint16_t address) {
    if (address >= 0x2000) return 0;
    if (CHR_size == 0) {
        if (mapper == 0 || mapper == 2) {
            return CHR_ram.at(address);
        }
        else if (mapper == 1) {
            if (MMC1reg.CHRmode == 0) {
                return CHR_ram.at((KILO * 4u * current_chr_bank1)+address);
            } else {
                if (address <= 0x0FFF) {
                    return CHR_ram.at((KILO * 4u * current_chr_bank1)+address);
                } else if (address <= 0x1FFF) {
                    return CHR_ram.at((KILO * 4u * current_chr_bank2)+(address - 0x1000));
                }
            }
        }
    }
    else {
        if (mapper == 0 || mapper == 2) {
            return raw_rom_data[address + CHR_rom_data];
        }
        else if (mapper == 1) {
            if (MMC1reg.CHRmode == 0) {
                return raw_rom_data[(KILO * 4u * current_chr_bank1)+address];
            } else {
                if (address <= 0x0FFF) {
                    return raw_rom_data[(KILO * 4u * current_chr_bank1)+address];
                } else if (address <= 0x1FFF) {
                    return raw_rom_data[(KILO * 4u * current_chr_bank2)+(address - 0x1000)];
                }
            }
        }
    }
    return 0;
}

uint16_t Gamepak::translate_nametable_address(uint16_t address) {
    if (mapper == 0 || mapper == 2) {
        switch (headers->byte6.mirroring) {
            case 0: // horizontal
                if (address < 0x2800) {// Table A
                    return (uint16_t) (address % 0x400);
                } else { // Table B
                    return (uint16_t) ((address % 0x400) + 0x400);
                }
            case 1: // vertical
                return (uint16_t) (address % 0x800);
            default:
                return 0;
        }
    }
    if (mapper == 1) {
        switch (MMC1reg.mirroring) {
            case 0: // Single Low
                return (uint16_t)(address % 0x400);
            case 1: // Single High
                return (uint16_t) ((address % 0x400));
            case 2: // Vertical
                return (uint16_t) (address % 0x800);
            case 3: // Horizontal
                if (address < 0x2800) {// Table A
                    return (uint16_t) (address % 0x400);
                } else { // Table B
                    return (uint16_t) ((address % 0x400) + 0x400);
                }
            default:
                return address;
        }
    }
    else {
        return 0;
    }
}
