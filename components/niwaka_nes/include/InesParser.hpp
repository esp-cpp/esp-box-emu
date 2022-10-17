#pragma once
#include "common.hpp"
struct Header{
    uint8_t sign[4];
    uint8_t PRG_ROM_SIZE;   
    uint8_t CHR_ROM_SIZE;
    uint8_t Flags6;
    uint8_t Flags7;
    uint8_t PRG_ROM_SIZE_8K;
    uint8_t Flags9;
    uint8_t Flags10;
    uint8_t zero[5];//ここは0が埋まるだけ
}__attribute__((__packed__));


class InesParser:public Object{
    private:
        int file_size;
        struct Header ines_header;
        char* file_name;
        FILE* ines_stream;
        char* buff;
        char* prg_rom;
        char* chr_rom;
        int prg_rom_size;
        int chr_rom_size;
        int prg_rom_start_addr;
        int chr_rom_start_addr;
        int mapper_number;
        int GetFileSize();
        bool horizontal_mirror = false;
    public:
        InesParser(const char* file_name);
        ~InesParser();
        void Parse();
        char* GetChr(int index);
        int GetPrgSize();
        int GetChrSize();
        char* GetPrgRom();
        char* GetChrRom();
        bool IsHorizontal();
        int GetMapperNumber();
};
