#pragma once
#include "common.hpp"
using namespace std;
class Cpu;
class InstructionBase:public Object{
    protected:
        int nbytes;
        int cycles;
    public:
        string name;
        InstructionBase(string name, int nbytes, int cycles);
        virtual int Execute(Cpu* cpu) = 0;
        int GetCycle();
};

class Sei:public InstructionBase{
    public:
        Sei(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdxImmediate:public InstructionBase{
    public:
        LdxImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdyImmediate:public InstructionBase{
    public:
        LdyImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdyZeropageX:public InstructionBase{
    public:
        LdyZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdyZeropage:public InstructionBase{
    public:
        LdyZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdyAbsolute:public InstructionBase{
    public:
        LdyAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdyAbsoluteX:public InstructionBase{
    public:
        LdyAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdaImmediate:public InstructionBase{
    public:
        LdaImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdaAbsoluteX:public InstructionBase{
    public:
        LdaAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdaAbsoluteY:public InstructionBase{
    public:
        LdaAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdaZeropage:public InstructionBase{
    public:
        LdaZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdxZeropage:public InstructionBase{
    public:
        LdxZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdaZeropageX:public InstructionBase{
    public:
        LdaZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdaIndirectX:public InstructionBase{
    public:
        LdaIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdaIndirectY:public InstructionBase{
    public:
        LdaIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Txs:public InstructionBase{
    public:
        Txs(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Tsx:public InstructionBase{
    public:
        Tsx(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Tya:public InstructionBase{
    public:
        Tya(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Tay:public InstructionBase{
    public:
        Tay(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StaAbsolute:public InstructionBase{
    public:
        StaAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StaAbsoluteX:public InstructionBase{
    public:
        StaAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StaAbsoluteY:public InstructionBase{
    public:
        StaAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};


class StaIndirectX:public InstructionBase{
    public:
        StaIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StaIndirectY:public InstructionBase{
    public:
        StaIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StxAbsolute:public InstructionBase{
    public:
        StxAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StxZeropageY:public InstructionBase{
    public:
        StxZeropageY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StxZeropage:public InstructionBase{
    public:
        StxZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StyZeropage:public InstructionBase{
    public:
        StyZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StyZeropageX:public InstructionBase{
    public:
        StyZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StyAbsolute:public InstructionBase{
    public:
        StyAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StaZeropage:public InstructionBase{
    public:
        StaZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class StaZeropageX:public InstructionBase{
    public:
        StaZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Inx:public InstructionBase{
    public:
        Inx(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Iny:public InstructionBase{
    public:
        Iny(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IncAbsolute:public InstructionBase{
    public:
        IncAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IncAbsoluteX:public InstructionBase{
    public:
        IncAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IncZeropage:public InstructionBase{
    public:
        IncZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IncZeropageX:public InstructionBase{
    public:
        IncZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DecAbsolute:public InstructionBase{
    public:
        DecAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DecAbsoluteX:public InstructionBase{
    public:
        DecAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DecZeropageX:public InstructionBase{
    public:
        DecZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DecZeropage:public InstructionBase{
    public:
        DecZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Dey:public InstructionBase{
    public:
        Dey(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Dex:public InstructionBase{
    public:
        Dex(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Bne:public InstructionBase{
    public:
        Bne(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Bmi:public InstructionBase{
    public:
        Bmi(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Bcc:public InstructionBase{
    public:
        Bcc(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Beq:public InstructionBase{
    public:
        Beq(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Bcs:public InstructionBase{
    public:
        Bcs(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Bvs:public InstructionBase{
    public:
        Bvs(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Bvc:public InstructionBase{
    public:
        Bvc(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class JmpAbsolute:public InstructionBase{
    public:
        JmpAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class JmpIndirect:public InstructionBase{
    public:
        JmpIndirect(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};


class LdaAbsolute:public InstructionBase{
    public:
        LdaAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class BplRelative:public InstructionBase{
    public:
        BplRelative(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CpxImmediate:public InstructionBase{
    public:
        CpxImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CpxAbsolute:public InstructionBase{
    public:
        CpxAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CpyAbsolute:public InstructionBase{
    public:
        CpyAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CpxZeropage:public InstructionBase{
    public:
        CpxZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CpyImmediate:public InstructionBase{
    public:
        CpyImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CpyZeropage:public InstructionBase{
    public:
        CpyZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AndImmediate:public InstructionBase{
    public:
        AndImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AndAbsoluteX:public InstructionBase{
    public:
        AndAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AndZeropageX:public InstructionBase{
    public:
        AndZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AndAbsolute:public InstructionBase{
    public:
        AndAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AndAbsoluteY:public InstructionBase{
    public:
        AndAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AndIndirectX:public InstructionBase{
    public:
        AndIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AndIndirectY:public InstructionBase{
    public:
        AndIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AndZeropage:public InstructionBase{
    public:
        AndZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AdcZeropage:public InstructionBase{
    public:
        AdcZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AdcImmediate:public InstructionBase{
    public:
        AdcImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AdcAbsoluteX:public InstructionBase{
    public:
        AdcAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AdcZeropageX:public InstructionBase{
    public:
        AdcZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AdcAbsoluteY:public InstructionBase{
    public:
        AdcAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AdcIndirectY:public InstructionBase{
    public:
        AdcIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AdcAbsolute:public InstructionBase{
    public:
        AdcAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AdcIndirectX:public InstructionBase{
    public:
        AdcIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};


class JsrAbsolute:public InstructionBase{
    public:
        JsrAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Rts:public InstructionBase{
    public:
        Rts(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Rti:public InstructionBase{
    public:
        Rti(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class EorImmediate:public InstructionBase{
    public:
        EorImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class EorAbsoluteX:public InstructionBase{
    public:
        EorAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class EorZeropageX:public InstructionBase{
    public:
        EorZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};


class EorAbsoluteY:public InstructionBase{
    public:
        EorAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class EorIndirectY:public InstructionBase{
    public:
        EorIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};


class EorAbsolute:public InstructionBase{
    public:
        EorAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class EorIndirectX:public InstructionBase{
    public:
        EorIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class EorZeropage:public InstructionBase{
    public:
        EorZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdxAbsoluteY:public InstructionBase{
    public:
        LdxAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdxAbsolute:public InstructionBase{
    public:
        LdxAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LdxZeropageY:public InstructionBase{
    public:
        LdxZeropageY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Cld:public InstructionBase{
    public:
        Cld(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Clc:public InstructionBase{
    public:
        Clc(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Pha:public InstructionBase{
    public:
        Pha(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Php:public InstructionBase{
    public:
        Php(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Pla:public InstructionBase{
    public:
        Pla(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Plp:public InstructionBase{
    public:
        Plp(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Sec:public InstructionBase{
    public:
        Sec(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SbcZeropage:public InstructionBase{
    public:
        SbcZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SbcImmediate:public InstructionBase{
    public:
        SbcImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SbcAbsoluteX:public InstructionBase{
    public:
        SbcAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SbcZeropageX:public InstructionBase{
    public:
        SbcZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SbcAbsoluteY:public InstructionBase{
    public:
        SbcAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SbcIndirectY:public InstructionBase{
    public:
        SbcIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SbcAbsolute:public InstructionBase{
    public:
        SbcAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SbcIndirectX:public InstructionBase{
    public:
        SbcIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Tax:public InstructionBase{
    public:
        Tax(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Txa:public InstructionBase{
    public:
        Txa(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CmpImmediate:public InstructionBase{
    public:
        CmpImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CmpAbsoluteX:public InstructionBase{
    public:
        CmpAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};


class CmpZeropageX:public InstructionBase{
    public:
        CmpZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CmpAbsoluteY:public InstructionBase{
    public:
        CmpAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CmpIndirectY:public InstructionBase{
    public:
        CmpIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CmpAbsolute:public InstructionBase{
    public:
        CmpAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CmpZeropage:public InstructionBase{
    public:
        CmpZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class CmpIndirectX:public InstructionBase{
    public:
        CmpIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class OraImmediate:public InstructionBase{
    public:
        OraImmediate(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class OraZeropageX:public InstructionBase{
    public:
        OraZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class OraIndirectY:public InstructionBase{
    public:
        OraIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class OraZeropage:public InstructionBase{
    public:
        OraZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class OraAbsoluteX:public InstructionBase{
    public:
        OraAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class OraAbsoluteY:public InstructionBase{
    public:
        OraAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class OraAbsolute:public InstructionBase{
    public:
        OraAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class OraIndirectX:public InstructionBase{
    public:
        OraIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LsrAccumulator:public InstructionBase{
    public:
        LsrAccumulator(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LsrZeropage:public InstructionBase{
    public:
        LsrZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LsrZeropageX:public InstructionBase{
    public:
        LsrZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LsrAbsolute:public InstructionBase{
    public:
        LsrAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LsrAbsoluteX:public InstructionBase{
    public:
        LsrAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AslAccumulator:public InstructionBase{
    public:
        AslAccumulator(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AslAbsoluteX:public InstructionBase{
    public:
        AslAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AslZeropage:public InstructionBase{
    public:
        AslZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AslAbsolute:public InstructionBase{
    public:
        AslAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class AslZeropageX:public InstructionBase{
    public:
        AslZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RorAccumulator:public InstructionBase{
    public:
        RorAccumulator(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RorZeropage:public InstructionBase{
    public:
        RorZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RorAbsolute:public InstructionBase{
    public:
        RorAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RorZeropageX:public InstructionBase{
    public:
        RorZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RorAbsoluteX:public InstructionBase{
    public:
        RorAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RolAccumulator:public InstructionBase{
    public:
        RolAccumulator(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RolZeropage:public InstructionBase{
    public:
        RolZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RolAbsolute:public InstructionBase{
    public:
        RolAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RolAbsoluteX:public InstructionBase{
    public:
        RolAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RolZeropageX:public InstructionBase{
    public:
        RolZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Nop:public InstructionBase{
    public:
        Nop(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class NopD:public InstructionBase{
    public:
        NopD(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class NopDx:public InstructionBase{
    public:
        NopDx(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class NopAx:public InstructionBase{
    public:
        NopAx(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class BitZeropage:public InstructionBase{
    public:
        BitZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class BitAbsolute:public InstructionBase{
    public:
        BitAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Sed:public InstructionBase{
    public:
        Sed(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class Clv:public InstructionBase{
    public:
        Clv(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LaxIndirectX:public InstructionBase{
    public:
        LaxIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LaxZeropage:public InstructionBase{
    public:
        LaxZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LaxAbsolute:public InstructionBase{
    public:
        LaxAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LaxIndirectY:public InstructionBase{
    public:
        LaxIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LaxZeropageY:public InstructionBase{
    public:
        LaxZeropageY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class LaxAbsoluteY:public InstructionBase{
    public:
        LaxAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SaxIndirectX:public InstructionBase{
    public:
        SaxIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SaxZeropage:public InstructionBase{
    public:
        SaxZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SaxAbsolute:public InstructionBase{
    public:
        SaxAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SaxZeropageY:public InstructionBase{
    public:
        SaxZeropageY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DcpIndirectX:public InstructionBase{
    public:
        DcpIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DcpIndirectY:public InstructionBase{
    public:
        DcpIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DcpZeropage:public InstructionBase{
    public:
        DcpZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DcpZeropageX:public InstructionBase{
    public:
        DcpZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DcpAbsolute:public InstructionBase{
    public:
        DcpAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DcpAbsoluteY:public InstructionBase{
    public:
        DcpAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class DcpAbsoluteX:public InstructionBase{
    public:
        DcpAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IscIndirectX:public InstructionBase{
    public:
        IscIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IscIndirectY:public InstructionBase{
    public:
        IscIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IscZeropage:public InstructionBase{
    public:
        IscZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IscZeropageX:public InstructionBase{
    public:
        IscZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IscAbsolute:public InstructionBase{
    public:
        IscAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IscAbsoluteY:public InstructionBase{
    public:
        IscAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class IscAbsoluteX:public InstructionBase{
    public:
        IscAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SloIndirectX:public InstructionBase{
    public:
        SloIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SloZeropage:public InstructionBase{
    public:
        SloZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SloAbsolute:public InstructionBase{
    public:
        SloAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SloIndirectY:public InstructionBase{
    public:
        SloIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SloZeropageX:public InstructionBase{
    public:
        SloZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SloAbsoluteY:public InstructionBase{
    public:
        SloAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SloAbsoluteX:public InstructionBase{
    public:
        SloAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RlaIndirectX:public InstructionBase{
    public:
        RlaIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RlaZeropage:public InstructionBase{
    public:
        RlaZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RlaAbsolute:public InstructionBase{
    public:
        RlaAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RlaIndirectY:public InstructionBase{
    public:
        RlaIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RlaZeropageX:public InstructionBase{
    public:
        RlaZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RlaAbsoluteY:public InstructionBase{
    public:
        RlaAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RlaAbsoluteX:public InstructionBase{
    public:
        RlaAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SreIndirectX:public InstructionBase{
    public:
        SreIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SreZeropage:public InstructionBase{
    public:
        SreZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SreAbsolute:public InstructionBase{
    public:
        SreAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SreIndirectY:public InstructionBase{
    public:
        SreIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SreZeropageX:public InstructionBase{
    public:
        SreZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SreAbsoluteY:public InstructionBase{
    public:
        SreAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class SreAbsoluteX:public InstructionBase{
    public:
        SreAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RraIndirectX:public InstructionBase{
    public:
        RraIndirectX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RraIndirectY:public InstructionBase{
    public:
        RraIndirectY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RraZeropage:public InstructionBase{
    public:
        RraZeropage(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RraAbsolute:public InstructionBase{
    public:
        RraAbsolute(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RraZeropageX:public InstructionBase{
    public:
        RraZeropageX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RraAbsoluteY:public InstructionBase{
    public:
        RraAbsoluteY(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};

class RraAbsoluteX:public InstructionBase{
    public:
        RraAbsoluteX(string name, int nbytes, int cycles);
        int Execute(Cpu* cpu);
};