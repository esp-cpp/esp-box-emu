#include "Instruction.hpp"
#include "InterruptManager.hpp"
#include "JoyPad.hpp"
#include "common.hpp"
#include "InesParser.hpp"
#include "Memory.hpp"
#include "Ppu.hpp"
#include "Cpu.hpp"
#define SIGN_FLG 0x80
#define STACK_BASE_ADDR 0x0100

Cpu::Cpu(Bus* bus){
    this->bus = bus;
    assert(this->bus!=NULL);
    this->pc = 0x8000;

    for(int i=0; i<this->instruction_size; i++){
        this->instructions[i] = NULL;
    }
    this->instructions[0x01] = new OraIndirectX("OraIndirectX", 2, 6);
    this->instructions[0x03] = new SloIndirectX("SloIndirectX", 2, 8);
    this->instructions[0x04] = new NopD("NopD", 1, 2);
    this->instructions[0x05] = new OraZeropage("OraZeropage", 2, 3);
    this->instructions[0x06] = new AslZeropage("AslZeropage", 2, 5);
    this->instructions[0x07] = new SloZeropage("SloZeropage", 2, 5);
    this->instructions[0x08] = new Php("Php", 1, 3);
    this->instructions[0x09] = new OraImmediate("OraImmediate", 2, 2);
    this->instructions[0x0A] = new AslAccumulator("AslAccumulator", 1, 2);
    this->instructions[0x0C] = new NopAx("NopAx", 1, 2);
    this->instructions[0x0D] = new OraAbsolute("OraAbsolute", 3, 4);
    this->instructions[0x0E] = new AslAbsolute("AslAbsolute", 3, 6);
    this->instructions[0x0F] = new SloAbsolute("SloAbsolute", 3, 6);
    this->instructions[0x10] = new BplRelative("BplRelative", 2, 2);
    this->instructions[0x11] = new OraIndirectY("OraIndirectY", 2, 5);
    this->instructions[0x13] = new SloIndirectY("SloIndirectY", 2, 8);
    this->instructions[0x14] = new NopDx("NopDx", 1, 2);
    this->instructions[0x15] = new OraZeropageX("OraZeropageX", 2, 4);
    this->instructions[0x16] = new AslZeropageX("AslZeropageX", 2, 6);
    this->instructions[0x17] = new SloZeropageX("SloZeropageX", 2, 6);
    this->instructions[0x18] = new Clc("Clc", 1, 2);
    this->instructions[0x19] = new OraAbsoluteY("OraAbsoluteY", 3, 4);
    this->instructions[0x1C] = new NopAx("NopAx", 1, 2);
    this->instructions[0x1A] = new Nop("Nop", 1, 2);
    this->instructions[0x1B] = new SloAbsoluteY("SloAbsoluteY", 3, 7);
    this->instructions[0x1D] = new OraAbsoluteX("OraAbsoluteX", 3, 4);
    this->instructions[0x1E] = new AslAbsoluteX("AslAbsoluteX", 3, 7);
    this->instructions[0x1F] = new SloAbsoluteX("SloAbsoluteX", 3, 7);
    this->instructions[0x20] = new JsrAbsolute("JsrAbsolute", 3, 6);
    this->instructions[0x21] = new AndIndirectX("AndIndirectX", 2, 6);
    this->instructions[0x23] = new RlaIndirectX("RlaIndirectX", 2, 8);
    this->instructions[0x24] = new BitZeropage("BitZeropage", 2, 3);
    this->instructions[0x25] = new AndZeropage("AndZeropage", 2, 3);
    this->instructions[0x26] = new RolZeropage("RolZeropage", 2, 5);
    this->instructions[0x27] = new RlaZeropage("RlaZeropage", 2, 5);
    this->instructions[0x28] = new Plp("Plp", 1, 4);
    this->instructions[0x29] = new AndImmediate("AndImmediate", 2, 2);
    this->instructions[0x2A] = new RolAccumulator("RolAccumulator", 1, 2);
    this->instructions[0x2C] = new BitAbsolute("BitAbsolute", 3, 4);
    this->instructions[0x2D] = new AndAbsolute("AndAbsolute", 3, 4);
    this->instructions[0x2E] = new RolAbsolute("RolAbsolute", 3, 6);
    this->instructions[0x2F] = new RlaAbsolute("RlaAbsolute", 3, 6);
    this->instructions[0x30] = new Bmi("Bmi", 2, 2);
    this->instructions[0x31] = new AndIndirectY("AndIndirectY", 2, 5);
    this->instructions[0x33] = new RlaIndirectY("RlaIndirectY", 2, 8);
    this->instructions[0x34] = new NopDx("NopDx", 1, 2);
    this->instructions[0x35] = new AndZeropageX("AndZeropageX", 2, 4);
    this->instructions[0x36] = new RolZeropageX("RolZeropageX", 2, 6);
    this->instructions[0x37] = new RlaZeropageX("RlaZeropageX", 2, 6);
    this->instructions[0x38] = new Sec("Sec", 1, 2);
    this->instructions[0x39] = new AndAbsoluteY("AndAbsoluteY", 3, 4);
    this->instructions[0x3A] = new Nop("Nop", 1, 2);
    this->instructions[0x3B] = new RlaAbsoluteY("RlaAbsoluteY", 3, 7);
    this->instructions[0x3C] = new NopAx("NopAx", 1, 2);
    this->instructions[0x3E] = new RolAbsoluteX("RolAbsoluteX", 3, 7);
    this->instructions[0x3F] = new RlaAbsoluteX("RlaAbsoluteX", 3, 7);
    this->instructions[0x3D] = new AndAbsoluteX("AndAbsoluteX", 3, 4);
    this->instructions[0x40] = new Rti("Rti", 1, 6);
    this->instructions[0x41] = new EorIndirectX("EorIndirectX", 2, 6);
    this->instructions[0x43] = new SreIndirectX("SreIndirectX", 2, 8);
    this->instructions[0x44] = new NopD("NopD", 1, 2);
    this->instructions[0x45] = new EorZeropage("EorZeropage", 2, 3);
    this->instructions[0x46] = new LsrZeropage("LsrZeropage", 2, 5);
    this->instructions[0x47] = new SreZeropage("SreZeropage", 2, 5);
    this->instructions[0x48] = new Pha("Pha", 1, 3);
    this->instructions[0x49] = new EorImmediate("EorImmediate", 2, 2);
    this->instructions[0x4A] = new LsrAccumulator("LsrAccumulator", 1, 2);
    this->instructions[0x4C] = new JmpAbsolute("JmpAbsolute", 3, 3);
    this->instructions[0x4D] = new EorAbsolute("EorAbsolute", 3, 4);
    this->instructions[0x4E] = new LsrAbsolute("LsrAbsolute", 3, 6);
    this->instructions[0x4F] = new SreAbsolute("SreAbsolute", 3, 6);
    this->instructions[0x50] = new Bvc("Bvc", 2, 2);
    this->instructions[0x51] = new EorIndirectY("EorIndirectY", 2, 5);
    this->instructions[0x53] = new SreIndirectY("SreIndirectY", 2, 8);
    this->instructions[0x54] = new NopDx("NopDx", 1, 2);
    this->instructions[0x55] = new EorZeropageX("EorZeropageX", 2, 4);
    this->instructions[0x56] = new LsrZeropageX("LsrZeropageX", 2, 6);
    this->instructions[0x57] = new SreZeropageX("SreZeropageX", 5, 7);
    this->instructions[0x59] = new EorAbsoluteY("EorAbsoluteY", 3, 4);
    this->instructions[0x5A] = new Nop("Nop", 1, 2);
    this->instructions[0x5B] = new SreAbsoluteY("SreAbsoluteY", 3, 7);
    this->instructions[0x5C] = new NopAx("NopAx", 1, 2);
    this->instructions[0x5D] = new EorAbsoluteX("EorAbsoluteX", 3, 4);
    this->instructions[0x5E] = new LsrAbsoluteX("LsrAbsoluteX", 3, 7);
    this->instructions[0x5F] = new SreAbsoluteX("SreAbsoluteX", 2, 8);
    this->instructions[0x60] = new Rts("Rts", 1, 6);
    this->instructions[0x61] = new AdcIndirectX("AdcIndirectX", 2, 6);
    this->instructions[0x63] = new RraIndirectX("RraIndirectX", 2, 8);
    this->instructions[0x64] = new NopD("NopD", 1, 2);
    this->instructions[0x65] = new AdcZeropage("AdcZeropage", 2, 3);
    this->instructions[0x66] = new RorZeropage("RorZeropage", 2, 5);
    this->instructions[0x67] = new RraZeropage("RraZeropage", 2, 5);
    this->instructions[0x68] = new Pla("Pla", 1, 4);
    this->instructions[0x69] = new AdcImmediate("AdcImmediate", 2, 2);
    this->instructions[0x6A] = new RorAccumulator("RorAccumulator", 1, 2);
    this->instructions[0x6C] = new JmpIndirect("JmpIndirect", 3, 5);
    this->instructions[0x6D] = new AdcAbsolute("AdcAbsolute", 3, 4);
    this->instructions[0x6E] = new RorAbsolute("RorAbsolute", 3, 6); 
    this->instructions[0x6F] = new RraAbsolute("RraAbsolute", 3, 6);
    this->instructions[0x70] = new Bvs("Bvs", 2, 2);
    this->instructions[0x71] = new AdcIndirectY("AdcIndirectY", 2, 5);
    this->instructions[0x73] = new RraIndirectY("RraIndirectY", 2, 8);
    this->instructions[0x74] = new NopDx("NopDx", 1, 2);
    this->instructions[0x75] = new AdcZeropageX("AdcZeropageX", 2, 4);
    this->instructions[0x76] = new RorZeropageX("RorZeropageX", 2, 6);
    this->instructions[0x77] = new RraZeropageX("RraZeropageX", 2, 6);
    this->instructions[0x78] = new Sei("SEI", 1, 2);
    this->instructions[0x79] = new AdcAbsoluteY("AdcAbsoluteY", 3, 4);
    this->instructions[0x7A] = new Nop("Nop", 1, 2);
    this->instructions[0x7B] = new RraAbsoluteY("RraAbsoluteY", 3, 7);
    this->instructions[0x7C] = new NopAx("NopAx", 1, 2);
    this->instructions[0x7D] = new AdcAbsoluteX("AdcAbsoluteX", 3, 4);
    this->instructions[0x7E] = new RorAbsoluteX("RorAbsoluteX", 3, 7);
    this->instructions[0x7F] = new RraAbsoluteX("RraAbsoluteX", 3, 7);
    this->instructions[0x80] = new NopD("NopD", 1, 2);
    this->instructions[0x81] = new StaIndirectX("StaIndirectX", 2, 6);
    this->instructions[0x83] = new SaxIndirectX("SaxIndirectX", 2, 6);
    this->instructions[0x84] = new StyZeropage("StyZeropage", 2, 3);
    this->instructions[0x85] = new StaZeropage("StaZeropage", 2, 3);
    this->instructions[0x86] = new StxZeropage("StxZeropage", 2, 3);
    this->instructions[0x87] = new SaxZeropage("SaxZeropage", 2, 3);
    this->instructions[0x88] = new Dey("Dey", 1, 2);
    this->instructions[0x8A] = new Txa("Txa", 1, 2);
    this->instructions[0x8C] = new StyAbsolute("StyAbsolute", 3, 4);
    this->instructions[0x8D] = new StaAbsolute("StaAbsolute", 3, 4);
    this->instructions[0x8E] = new StxAbsolute("StxAbsolute", 3, 4);
    this->instructions[0x8F] = new SaxAbsolute("SaxAbsolute", 3, 4);
    this->instructions[0x90] = new Bcc("Bcc", 2, 2);
    this->instructions[0x91] = new StaIndirectY("StaIndirectY", 2, 6);
    this->instructions[0x94] = new StyZeropageX("StyZeropageX", 2, 4);
    this->instructions[0x95] = new StaZeropageX("StaZeropageX", 2, 4);
    this->instructions[0x96] = new StxZeropageY("StxZeropageY", 2, 4);
    this->instructions[0x97] = new SaxZeropageY("SaxZeropageY", 2, 4);
    this->instructions[0x98] = new Tya("Tya", 1, 2);
    this->instructions[0x99] = new StaAbsoluteY("StaAbsoluteY", 3, 5);
    this->instructions[0x9A] = new Txs("Txs", 1, 2);
    this->instructions[0x9D] = new StaAbsoluteX("StaAbsoluteX", 3, 5);
    this->instructions[0xA0] = new LdyImmediate("LdyImmediate", 2, 2);
    this->instructions[0xA1] = new LdaIndirectX("LdaIndirectX", 2, 6);
    this->instructions[0xA2] = new LdxImmediate("LdxImmediate", 2, 2);
    this->instructions[0xA3] = new LaxIndirectX("LaxIndirectX", 2, 6);
    this->instructions[0xA4] = new LdyZeropage("LdyZeropage", 2, 3);
    this->instructions[0xA5] = new LdaZeropage("LdaZeropage", 2, 3);
    this->instructions[0xA6] = new LdxZeropage("LdxZeropage", 2, 3);
    this->instructions[0xA7] = new LaxZeropage("LaxZeropage", 2, 3);
    this->instructions[0xA8] = new Tay("Tay", 2, 3);
    this->instructions[0xA9] = new LdaImmediate("LdaImmediate", 2, 2);
    this->instructions[0xAA] = new Tax("Tax", 1, 2);
    this->instructions[0xAC] = new LdyAbsolute("LdyAbsolute", 3, 4);
    this->instructions[0xAD] = new LdaAbsolute("LdaAbsolute", 3, 4);
    this->instructions[0xAE] = new LdxAbsolute("LdxAbsolute", 3, 4);
    this->instructions[0xAF] = new LaxAbsolute("LaxAbsolute", 3, 4);
    this->instructions[0xB0] = new Bcs("Bcs", 2, 2);
    this->instructions[0xB1] = new LdaIndirectY("LdaIndirectY", 2, 5);
    this->instructions[0xB3] = new LaxIndirectY("LaxIndirectY", 2, 5);
    this->instructions[0xB4] = new LdyZeropageX("LdyZeropageX", 2, 4);
    this->instructions[0xB5] = new LdaZeropageX("LdaZeropageX", 2, 4);
    this->instructions[0xB6] = new LdxZeropageY("LdxZeropageY", 2, 4);
    this->instructions[0xB7] = new LaxZeropageY("LaxZeropageY", 2, 4);
    this->instructions[0xB8] = new Clv("Clv", 1, 2);
    this->instructions[0xB9] = new LdaAbsoluteY("LdaAbsoluteY", 3 , 4);
    this->instructions[0xBA] = new Tsx("Tsx", 1, 2);
    this->instructions[0xBC] = new LdyAbsoluteX("LdyAbsoluteX", 3, 4);
    this->instructions[0xBD] = new LdaAbsoluteX("LdaAbsoluteX", 3, 4);
    this->instructions[0xBE] = new LdxAbsoluteY("LdxAbsoluteY", 3, 4);
    this->instructions[0xBF] = new LaxAbsoluteY("LaxAbsoluteY", 3, 4);
    this->instructions[0xC0] = new CpyImmediate("CpyImmediate", 2, 2);
    this->instructions[0xC1] = new CmpIndirectX("CmpIndirectX", 2, 6);
    this->instructions[0xC3] = new DcpIndirectX("DcpIndirectX", 2, 8);
    this->instructions[0xC4] = new CpyZeropage("CpyZeropage", 2, 3);
    this->instructions[0xC5] = new CmpZeropage("CmpZeropage", 2, 3);
    this->instructions[0xC6] = new DecZeropage("DecZeropage", 2, 5);
    this->instructions[0xC7] = new DcpZeropage("DcpZeropage", 2, 5);
    this->instructions[0xC8] = new Iny("Iny", 1, 2);
    this->instructions[0xC9] = new CmpImmediate("CmpImmediate", 2, 2);
    this->instructions[0xCA] = new Dex("Dex", 1, 2);
    this->instructions[0xCC] = new CpyAbsolute("CpyAbsolute", 3, 4);
    this->instructions[0xCD] = new CmpAbsolute("CmpAbsolute", 3, 4);
    this->instructions[0xCE] = new DecAbsolute("DecAbsolute", 3, 6);
    this->instructions[0xCF] = new DcpAbsolute("DcpAbsolute", 2, 5);
    this->instructions[0xD0] = new Bne("Bne", 2, 2);
    this->instructions[0xD1] = new CmpIndirectY("CmpIndirectY", 2, 5);
    this->instructions[0xD3] = new DcpIndirectY("DcpIndirectY", 2, 8);
    this->instructions[0xD4] = new NopDx("NopDx", 1, 2);
    this->instructions[0xD5] = new CmpZeropageX("CmpZeropageX", 2, 4);
    this->instructions[0xD6] = new DecZeropageX("DecZeropageX", 2, 6);
    this->instructions[0xD7] = new DcpZeropageX("DcpZeropageX", 2, 6);
    this->instructions[0xD8] = new Cld("Cld", 1, 2);
    this->instructions[0xD9] = new CmpAbsoluteY("CmpAbsoluteY", 3, 4);
    this->instructions[0xDA] = new Nop("Nop", 1, 2);
    this->instructions[0xDB] = new DcpAbsoluteY("DcpAbsoluteY", 3, 7);
    this->instructions[0xDC] = new NopAx("NopAx", 1, 2);
    this->instructions[0xDD] = new CmpAbsoluteX("CmpAbsoluteX", 3, 4);
    this->instructions[0xDE] = new DecAbsoluteX("DecAbsoluteX", 3, 7);
    this->instructions[0xDF] = new DcpAbsoluteX("DcpAbsoluteX", 3, 7);
    this->instructions[0xE0] = new CpxImmediate("CpxImmediate", 2, 2);
    this->instructions[0xE1] = new SbcIndirectX("SbcIndirectX", 2, 6);
    this->instructions[0xE3] = new IscIndirectX("IscIndirectX", 2, 8);
    this->instructions[0xE4] = new CpxZeropage("CpxZeropage", 2, 3);
    this->instructions[0xE5] = new SbcZeropage("SbcZeropage", 2, 3);
    this->instructions[0xE6] = new IncZeropage("IncZeropage", 2, 5);
    this->instructions[0xE7] = new IscZeropage("IscZeropage", 2, 5);
    this->instructions[0xE8] = new Inx("Inx", 1, 2);
    this->instructions[0xE9] = new SbcImmediate("SbcImmediate", 2, 2);
    this->instructions[0xEA] = new Nop("Nop", 1, 2);
    this->instructions[0xEB] = new SbcImmediate("SbcImmediate", 2, 2);
    this->instructions[0xEC] = new CpxAbsolute("CpxAbsolute", 3, 4);
    this->instructions[0xED] = new SbcAbsolute("SbcAbsolute", 3, 4);
    this->instructions[0xEE] = new IncAbsolute("IncAbsolute", 3, 6);
    this->instructions[0xEF] = new IscAbsolute("IscAbsolute", 3, 6);
    this->instructions[0xF0] = new Beq("Beq", 2, 2);
    this->instructions[0xF1] = new SbcIndirectY("SbcIndirectY", 2, 5);
    this->instructions[0xF3] = new IscIndirectY("IscIndirectY", 2, 4);
    this->instructions[0xF4] = new NopDx("NopDx", 1, 2);
    this->instructions[0xF5] = new SbcZeropageX("SbcZeropageX", 2, 4);
    this->instructions[0xF6] = new IncZeropageX("IncZeropageX", 2, 6);
    this->instructions[0xF7] = new IscZeropageX("IscZeropageX", 2, 6);
    this->instructions[0xF8] = new Sed("Sed", 1, 2);
    this->instructions[0xF9] = new SbcAbsoluteY("SbcAbsoluteY", 3, 4);
    this->instructions[0xFA] = new Nop("Nop", 1, 2);
    this->instructions[0xFB] = new IscAbsoluteY("IscAbsoluteY", 3, 7);
    this->instructions[0xFC] = new NopAx("NopAx", 1, 2);
    this->instructions[0xFD] = new SbcAbsoluteX("SbcAbsoluteX", 3, 4);
    this->instructions[0xFE] = new IncAbsoluteX("IncAbsoluteX", 3, 7);
    this->instructions[0xFF] = new IscAbsoluteX("IscAbsoluteX", 3, 7);
    this->P.raw = 0x24;
    //this->gprs[S_KIND] = 0x01FD;
    this->gprs[S_KIND] = 0xFD;
    this->gprs[A_KIND] = 0x00;
    this->gprs[X_KIND] = 0x00;
    this->gprs[Y_KIND] = 0x00;
}

int Cpu::Execute(){
    unsigned char op_code;
    static int idx = 1;
    op_code = this->bus->Read8(this->pc);
    int cycles;
    if(this->instructions[op_code]==NULL){
        this->ShowSelf();
        this->Error("Not implemented: op_code=%02X", op_code);
    }
    this->pc++;
    idx++;
    return this->instructions[op_code]->Execute(this);
}

void Cpu::SetI(){
    this->P.flgs.I = 1;
}

void Cpu::SetN(){
    this->P.flgs.N = 1;
}

void Cpu::SetZ(){
    this->P.flgs.Z = 1;
}

void Cpu::SetC(){
    this->P.flgs.C = 1;
}

void Cpu::SetV(){
    this->P.flgs.V = 1;
}

void Cpu::SetB(){
    this->P.flgs.B = 1;
}

void Cpu::ClearI(){
    this->P.flgs.I = 0;
}

void Cpu::ClearN(){
    this->P.flgs.N = 0;
}

void Cpu::ClearZ(){
    this->P.flgs.Z = 0;
}

void Cpu::ClearC(){
    this->P.flgs.C = 0;
}

void Cpu::ClearV(){
    this->P.flgs.V = 0;
}

uint8_t Cpu::GetGprValue(REGISTER_KIND register_kind){
    return this->gprs[register_kind];
}

void Cpu::AddPc(uint16_t value){
    this->pc = this->pc + value;
}

void Cpu::SetPc(uint16_t value){
    this->pc = value;
}

uint8_t Cpu::Read8(uint16_t addr){
    return this->bus->Read8(addr);
}

uint16_t Cpu::Read16(uint16_t addr){
    uint16_t value;
    value = ((uint16_t)this->bus->Read8(addr)) | (((uint16_t)this->bus->Read8(addr+1))<<8);
    return value;
}

uint16_t Cpu::GetPc(){
    return this->pc;
}

void Cpu::Set8(REGISTER_KIND register_kind, uint8_t value){
    this->gprs[register_kind] = value;
}

void Cpu::ShowSelf(){
    fprintf(stderr, "A_REGISTER  : %02X\n", this->gprs[A_KIND]);
    fprintf(stderr, "X_REGISTER  : %02X\n", this->gprs[X_KIND]);
    fprintf(stderr, "Y_REGISTER  : %02X\n", this->gprs[Y_KIND]);
    fprintf(stderr, "S_REGISTER  : %02X\n", this->gprs[S_KIND]);
    fprintf(stderr, "P_REGISTER  : %02X\n", this->P.raw);
    fprintf(stderr, "PC_REGISTER : %04X\n", this->pc);
}

void Cpu::UpdateNflg(uint8_t value){
    if((value&SIGN_FLG)==SIGN_FLG){
        this->P.flgs.N = 1;
        return;
    }
    this->P.flgs.N = 0;
}

void Cpu::UpdateZflg(uint8_t value){
    if(value==0x00){
        this->P.flgs.Z = 1;
        return;
    }
    this->P.flgs.Z = 0;
}

bool Cpu::IsNflg(){
    return this->P.flgs.N;
}

bool Cpu::IsZflg(){
    return this->P.flgs.Z;
}

bool Cpu::IsCflg(){
    return this->P.flgs.C;
}

bool Cpu::IsVflg(){
    return this->P.flgs.V;
}

uint8_t Cpu::GetCFLg(){
    return this->P.flgs.C;
}

void Cpu::Push8(uint8_t data){
    this->Write(((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR, data);
    this->gprs[S_KIND] -= 1;
}

void Cpu::Push16(uint16_t data){
    uint8_t *p = (uint8_t*)&data;
    //this->Write((((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR), *(p+1));//upper byte
    //this->Write((((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR)-1, *p);//lower byte
    this->Write((((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR)-1, data);
    this->gprs[S_KIND] -= 2;

    /***
    for(int i=0; i<sizeof(data); i++){
        this->Write((((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR), p[i]);
        this->gprs[S_KIND]--;
    }
    ***/
}

uint8_t Cpu::Pop8(){
    uint8_t data;
    this->gprs[S_KIND]++;
    return this->Read8(((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR);
}

uint16_t Cpu::Pop16(){
    uint16_t data;
    uint8_t* p = (uint8_t*)&data;

    *p = this->Read8((((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR)+1);//lowwer byte
    *(p+1) = this->Read8((((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR)+2);//upper byte
    this->gprs[S_KIND] += 2;

    /***
    for(int i=0; i<sizeof(data); i++){
        this->gprs[S_KIND]++;
        *(p+sizeof(data)-1-i) = this->Read8((((uint16_t)this->gprs[S_KIND])|STACK_BASE_ADDR));
    }
    ***/
    return data;
}

void Cpu::HandleNmi(InterruptManager* interrupt_manager){
    uint16_t pc;
    this->P.flgs.B = 0;
    this->P.flgs.I = 1;
    this->Push16(this->GetPc());
    this->Push8(this->P.raw);
    this->SetPc(this->Read16(0xFFFA));
    interrupt_manager->ClearNmi();
}

void Cpu::SetP(uint8_t value){
    this->P.raw = value;
}

void Cpu::SetD(){
    this->P.flgs.D = 1;
}

void Cpu::ClearD(){
    this->P.flgs.D = 0;
}

uint8_t Cpu::GetP(){
    return this->P.raw;
}

bool Cpu::CmpLastInstructionName(string name){
    if(this->instruction_log.size()<=1){
        return false;
    }
    return this->instruction_log[this->instruction_log.size()-2]==name;
}

void Cpu::Reset(){
    this->pc = this->Read16(0xFFFC);
    if(this->pc==0){
        this->pc = 0x8000;
    }
}