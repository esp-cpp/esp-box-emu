#include "InesParser.hpp"

InesParser::InesParser(const char* file_name){
    this->file_name = (char*)malloc(strlen(file_name)+1);
    strcpy(this->file_name, file_name);
    this->ines_stream = fopen(this->file_name, "rb");
    if(this->ines_stream==NULL){
        this->Error("%s : No such file", this->file_name);
    }
    this->file_size = this->GetFileSize();
    this->buff = (char*)malloc(this->file_size);
    fread(this->buff, 1, this->file_size, this->ines_stream);
    if(this->buff[0]!='N' || this->buff[1]!='E' || this->buff[2]!='S'){
        fprintf(stderr, "%02X\n", this->buff[0]);
        fprintf(stderr, "%02X\n", this->buff[1]);
        fprintf(stderr, "%02X\n", this->buff[2]);
        this->Error("Invalid input file");
    }
    this->Parse();
}

int InesParser::GetFileSize(){
    int file_size;
    fseek(this->ines_stream, 0, SEEK_END);
    file_size = ftell(this->ines_stream);
    fseek(this->ines_stream, 0, SEEK_SET);
    return file_size;
}

void InesParser::Parse(){
    memcpy(&this->ines_header, this->buff, sizeof(Header));
    this->prg_rom_size = this->ines_header.PRG_ROM_SIZE*0x4000;
    this->chr_rom_size = this->ines_header.CHR_ROM_SIZE*8192;
    if(this->ines_header.CHR_ROM_SIZE==0){
        this->Error("Not implemented: CHR_RAM");
    }
    this->prg_rom_start_addr = sizeof(Header);
    this->chr_rom_start_addr = sizeof(Header) + this->prg_rom_size;
    this->prg_rom = (char*)malloc(this->prg_rom_size);
    this->chr_rom = (char*)malloc(this->chr_rom_size);
    memcpy(this->prg_rom, this->buff+this->prg_rom_start_addr, this->prg_rom_size);
    memcpy(this->chr_rom, this->buff+this->chr_rom_start_addr, this->chr_rom_size);
    uint8_t *p = (uint8_t*)&this->ines_header;
    for(int i=0; i<sizeof(Header); i++){
        fprintf(stderr, "%02d : %02X\n", i, *(p+i));
    }
    if((*(p+6)&0x01)==0){
        this->horizontal_mirror = true;
    }else{
        this->horizontal_mirror = false;
    }
    if(*(p+6)&0x04){
        this->Error("Not implemented: Tranier");
    }
    this->mapper_number = ((p[6]>>4)&0x0F)|(p[7]&0xF0);
    fprintf(stderr, "mapper = %d\n", this->mapper_number);
    if(this->mapper_number!=0 && this->mapper_number!=3){
        this->Error("Not implemented : mapper_number = %d\n", this->mapper_number);
    }
    fprintf(stderr, "CHR_ROM_SIZE = %08X\n", this->ines_header.CHR_ROM_SIZE);
    fprintf(stderr, "PRG_ROM_SIZE = %08X\n", this->ines_header.PRG_ROM_SIZE);
}

char* InesParser::GetChr(int index){
    return this->chr_rom+16*index;
}

int InesParser::GetPrgSize(){
    return this->prg_rom_size;
}

int InesParser::GetChrSize(){
    return this->chr_rom_size;
}

char* InesParser::GetPrgRom(){
    return this->prg_rom;
}

char* InesParser::GetChrRom(){
    return this->chr_rom;
}

InesParser::~InesParser(){
    free(this->buff);
    free(this->prg_rom);
    free(this->chr_rom);
    fclose(this->ines_stream);
}

bool InesParser::IsHorizontal(){
    return this->horizontal_mirror;
}

int InesParser::GetMapperNumber(){
    return this->mapper_number;
}