// cpu.cpp contains the code to successfully emulate the 6502 that was inside the NES
// YAY FOR THIS BEING FUN!
#include "../include/cpu.h"
#include <iostream>
#include <iomanip>
//#define LOGGING

NesCpu::NesCpu(NesCPUMemory * memory) {
    this -> RAM = memory;
}

NesCpu::~NesCpu() {

}

void NesCpu::power_up() {
    this->cycles = nes_cpu_clock_t(7);
    this->registers.P = 0x24;
    this->registers.A = 0x00;
    this->registers.X = 0x00;
    this->registers.Y = 0x00;
    this->registers.S = 0xFD;
    this->registers.PC = this->RAM->read_word(RESET_INTERRUPT_VECTOR);
    this->crossedpage = false;
    this->IRQRequested = false;
    this->NMIRequested = false;
}

uint16_t NesCpu::getAddrBasedOnMode(AddressingMode mode) {
    uint16_t finaladdr = 0;
    switch (mode) {
        case ADDR_MODE_IMPLICIT: {
            finaladdr = 0;
            break;
        }
        case ADDR_MODE_ACCUMULATOR: {
            finaladdr = 0;
            break;
        }
        case ADDR_MODE_IMMEDIATE: {
            finaladdr = uint16_t(this->registers.PC++);
            break;
        }
        case ADDR_MODE_ZP: {
            finaladdr = this->RAM->read_byte(this->registers.PC++);
            break;
        }
        case ADDR_MODE_ZPX: {
           uint8_t addr = ((this->RAM->read_byte(this->registers.PC++)) + this->registers.X);
           finaladdr = addr;
           break;
        }
        case ADDR_MODE_ZPY: {
            uint8_t addr = ((this->RAM->read_byte(this->registers.PC++)) + this->registers.Y);
            finaladdr = addr;
            break;
        }
        case ADDR_MODE_RELATIVE: {
            int8_t offset = this->RAM->read_byte(this->registers.PC++);
            finaladdr = this->registers.PC;
            if (offset < 0) {
                finaladdr -= uint16_t(-offset);
            }
            else {
                finaladdr += uint16_t(offset);
            }
            break;
        }
        case ADDR_MODE_ABSOLUTE: {
            finaladdr = this->RAM->read_word(this->registers.PC);
            this->registers.PC += 2;
            break;
        }
        case ADDR_MODE_ABSOLUTEX: {
            uint16_t tempaddr = this->RAM->read_word(this->registers.PC);
            this->registers.PC += 2;
            finaladdr = tempaddr + this->registers.X;
            if ((tempaddr & 0xFF00) != (finaladdr & 0xFF00)) {
                this->crossedpage = true;
            }
            break;
        }
        case ADDR_MODE_ABSOLUTEY: {
            uint16_t tempaddr = this->RAM->read_word(this->registers.PC);
            this->registers.PC += 2;
            finaladdr = tempaddr + this->registers.Y;
            if ((tempaddr & 0xFF00) != (finaladdr & 0xFF00)) {
                this->crossedpage = true;
            }
            break;
        }
        case ADDR_MODE_INDIRECT: {
            uint16_t tempaddress = this->RAM->read_word(this->registers.PC);
            this->registers.PC += 2;
            finaladdr = this->RAM->read_word_page_bug(tempaddress);
            break;
        }
        case ADDR_MODE_INDIRECTX: {
            uint8_t tempaddress = this->RAM->read_byte(this->registers.PC++);
            tempaddress += this->registers.X;
            finaladdr = this->RAM->read_word_page_bug(tempaddress);
            break;
        }
        case ADDR_MODE_INDIRECTY: {
            uint8_t tempaddress = this->RAM->read_byte(this->registers.PC++);
            uint16_t tempaddr2 = this->RAM->read_word_page_bug(tempaddress);
            finaladdr = tempaddr2 + this->registers.Y;
            if ((finaladdr & 0xFF00) != (tempaddr2 & 0xFF00)) {
                this->crossedpage = true;
            }
            break;
        }
        default: {
            finaladdr = 0;
            break;
        }
    }
    return finaladdr;
}


nes_cpu_clock_t NesCpu::step() {

    nes_cpu_clock_t instructioncycles(0);

    if (this->NMIRequested) {
        instructioncycles += this->NMI();
    }

    if (!(TEST_INTERRUPT_DISABLE(this->registers.P))) {
        if (this->IRQRequested) {
            instructioncycles += this->IRQ();
        }
    }

    //Fetch Op Code
    uint16_t oldpc = this->registers.PC;
    uint8_t opcode = this->RAM->read_byte(this->registers.PC++);
    const Instruction *currentInstruction = &this->instructions[opcode];

    //Increment the cycle counter the base number of instructions
    instructioncycles += nes_cpu_clock_t(currentInstruction->baseNumCycles);

    if (currentInstruction->addrMode == INVALID_OPCODE) {
        std::cerr << "Invalid OpCode Used " << currentInstruction->name << " PC at: " << std::hex << "0x" << +oldpc << std::endl;
        return instructioncycles;
    }

    uint16_t address = getAddrBasedOnMode(currentInstruction->addrMode);

    if (this->crossedpage) {
        instructioncycles += nes_cpu_clock_t(currentInstruction->numPageCrossCycles);
        this->crossedpage = false;
    }

#ifdef LOGGING
    // 0         1         2         3         4         5         6         7         8
    // 0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567
    // C000  4C F5 C5  JMP $C5F5                       A:00 X:00 Y:00 P:24 SP:FD CYC:  0 SL:241

    std::cout << std::setfill('0') << std::setw(4) << std::right << std::hex << +oldpc;
    std::cout << " A:" << std::setfill('0') << std::setw(2) << std::right << std::hex << +this->registers.A;
    std::cout << " X:" << std::setfill('0') << std::setw(2) << std::right << std::hex << +this->registers.X;
    std::cout << " Y:" << std::setfill('0') << std::setw(2) << std::right << std::hex << +this->registers.Y;
    std::cout << " P:" << std::setfill('0') << std::setw(2) << std::right << std::hex << +this->registers.P;
    std::cout << " SP:" << std::setfill('0') << std::setw(2) << std::right << std::hex << +this->registers.S;
    std::cout << " CYC:" << std::dec << this->cycles.count() << std::endl;

#endif


    instructioncycles += currentInstruction->instFunc(address, this);

    this->cycles += instructioncycles;
    return instructioncycles;
}



void NesCpu::pushStackBtye(uint8_t data) {
    uint16_t address = uint16_t(STACK_BASE) + this->registers.S;
    this->RAM->write_byte(address, data);
    this->registers.S--;
}

uint8_t NesCpu::popStackByte() {
    this->registers.S++;
    uint8_t data = this->RAM->read_byte(uint16_t(STACK_BASE) + this->registers.S);
    return data;
}

void NesCpu::pushStackWord(uint16_t data) {
    uint16_t address = uint16_t(STACK_BASE) + this->registers.S;
    this->RAM->stack_write_word(address, data);
    this->registers.S-=2;
}

uint16_t NesCpu::popStackWord() {
    this->registers.S++;
    uint16_t data = this->RAM->read_word(uint16_t(STACK_BASE) +this->registers.S);
    this->registers.S++;
    return data;
}

void NesCpu::setFlags(uint8_t mask, bool set) {
    if (set) {
        this->registers.P |= mask;
    }
    else {
        this->registers.P &= ~mask;
    }
}

void NesCpu::requestNMI() {
    this->NMIRequested = true;
}



nes_cpu_clock_t NesCpu::NMI() {
    this->pushStackWord(this->registers.PC);
    uint8_t flags = this->registers.P;
    // Clear B Flag when generated by Interrupt
    flags &= ~B_MASK;
    this->pushStackBtye(flags);

    this->registers.PC = this->RAM->read_word(NMI_INTERRUPT_VECTOR);
    //Per NESDev Wiki http://wiki.nesdev.com/w/index.php/CPU_ALL
    this->setFlags(INTERRUPT_DISABLE_MASK, true);
    this->NMIRequested = false;

    return nes_cpu_clock_t(7);
}

nes_cpu_clock_t NesCpu::IRQ() {
    this->pushStackWord(this->registers.PC);
    uint8_t flags = this->registers.P;
    // Clear B flag when generated by Interrupt
    flags &= ~B_MASK;
    this->pushStackBtye(flags);

    this->registers.PC = this->RAM->read_word(IRQ_INTERRUPT_VECTOR);
    //Per NESDev Wiki http://wiki.nesdev.com/w/index.php/CPU_ALL
    this->setFlags(INTERRUPT_DISABLE_MASK, true);

    return nes_cpu_clock_t(7);
}

void NesCpu::updateZeroFlag(uint8_t value) {
    bool set = (value == 0);
    this->setFlags(ZERO_MASK, set);
}

void NesCpu::updateNegativeFlag(uint8_t value) {
    bool set = ((value&0x80) == 0x80);
    this->setFlags(NEGATIVE_MASK, set);
}

nes_cpu_clock_t NesCpu::getCycles() {
    return this->cycles;
}

nes_cpu_clock_t NesCpu::performBranch(uint16_t address) {
    nes_cpu_clock_t cycles(1);

    if ((this->registers.PC & 0xFF00) != (address & 0xFF00)) {
        cycles += nes_cpu_clock_t(1);
    }

    this->registers.PC = address;
    return cycles;
}

void NesCpu::setPC(uint16_t newAddress) {
    this->registers.PC = newAddress;
}



nes_cpu_clock_t brk(uint16_t address, NesCpu * cpu) {
    cpu->pushStackWord(cpu->registers.PC);
    uint8_t flags = cpu->registers.P;
    // Set B Flag when generated by Program
    flags |= B_MASK;
    cpu->pushStackBtye(flags);

    cpu->registers.PC = cpu->RAM->read_word(IRQ_INTERRUPT_VECTOR);
    //Per NESDev Wiki http://wiki.nesdev.com/w/index.php/CPU_ALL
    cpu->setFlags(INTERRUPT_DISABLE_MASK, true);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t ora(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);

    cpu->registers.A |= value;
    cpu->updateZeroFlag(cpu->registers.A);
    cpu->updateNegativeFlag(cpu->registers.A);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t slo(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = asl(address, cpu);
    cycles += ora(address, cpu);
    return cycles;
}

nes_cpu_clock_t dop(uint16_t address, NesCpu * cpu) {
    //NOOP
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t asl(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);
    bool set = ((value&0x80) == 0x80);
    cpu->setFlags(CARRY_MASK, set);
    value <<= 1;

    cpu->updateZeroFlag(value);
    cpu->updateNegativeFlag(value);

    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, value);

    return cycles;
}

nes_cpu_clock_t asla(uint16_t address, NesCpu * cpu) {
//    uint8_t value = cpu->registers.A;
    bool set = ((cpu->registers.A&0x80) == 0x80);
    cpu->setFlags(CARRY_MASK, set);
    cpu->registers.A <<= 1;

    cpu->updateZeroFlag(cpu->registers.A);
    cpu->updateNegativeFlag(cpu->registers.A);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t php(uint16_t address, NesCpu * cpu) {
    // B Flag Needs to be set per NESDev Wiki
    cpu->pushStackBtye((cpu->registers.P | uint8_t(B_MASK)));

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t anc(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = myand(address, cpu);
    cpu->setFlags(CARRY_MASK, TEST_NEGATIVE(cpu->registers.P));
    return cycles;
}

nes_cpu_clock_t top(uint16_t address, NesCpu * cpu) {
    //NOOP
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t bpl(uint16_t address, NesCpu * cpu) {
    if (!(TEST_NEGATIVE(cpu->registers.P))) {
        return cpu->performBranch(address);
    }
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t clc(uint16_t address, NesCpu * cpu) {
    cpu->setFlags(CARRY_MASK, false);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t nop(uint16_t address, NesCpu * cpu) {
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t jsr(uint16_t address, NesCpu * cpu) {
    cpu->pushStackWord(cpu->registers.PC-1);
    cpu->registers.PC = address;
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t myand(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);
    cpu->registers.A &= value;


    cpu->updateZeroFlag(cpu->registers.A);
    cpu->updateNegativeFlag(cpu->registers.A);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t rla(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = rol(address, cpu);
    cycles += myand(address, cpu);
    return cycles;
}

nes_cpu_clock_t bit(uint16_t address, NesCpu * cpu) {
   uint8_t value = cpu->RAM->read_byte(address);
   cpu->setFlags(NEGATIVE_MASK, ((value&0x80) != 0));
   cpu->setFlags(OVERFLOW_MASK, ((value&0x40) != 0));
   cpu->updateZeroFlag(value&cpu->registers.A);
   return nes_cpu_clock_t(0);
}

nes_cpu_clock_t rol(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);

    bool newCarryFlag = (value&0x80) != 0;
    value <<= 1;

    if (TEST_CARRY(cpu->registers.P)) {
        value |= 0x01;
    }

    cpu->setFlags(CARRY_MASK, newCarryFlag);
    cpu->updateZeroFlag(value);
    cpu->updateNegativeFlag(value);

    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, value);

    return cycles;

}

nes_cpu_clock_t rola(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->registers.A;
    bool newCarryFlag = (value&0x80) != 0;
    value <<= 1;

    if (TEST_CARRY(cpu->registers.P)) {
        value |= 0x01;
    }

    cpu->setFlags(CARRY_MASK, newCarryFlag);
    cpu->updateZeroFlag(value);
    cpu->updateNegativeFlag(value);

    cpu->registers.A = value;

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t plp(uint16_t address, NesCpu * cpu) {
    cpu->registers.P =  cpu->popStackByte();
    cpu->setFlags(B_MASK, false);
    cpu->setFlags(I_MASK, true);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t bmi(uint16_t address, NesCpu * cpu) {
    if (TEST_NEGATIVE(cpu->registers.P)) {
        return cpu->performBranch(address);
    }
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t sec(uint16_t address, NesCpu * cpu) {
    cpu->setFlags(CARRY_MASK, true);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t rti(uint16_t address, NesCpu * cpu) {
    cpu->registers.P = cpu->popStackByte();
    cpu->setFlags(B_MASK, false);
    cpu->setFlags(I_MASK, true);
    cpu->registers.PC = cpu->popStackWord();
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t eor(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);
    cpu->registers.A = cpu->registers.A^value;

    cpu->updateNegativeFlag(cpu->registers.A);
    cpu->updateZeroFlag(cpu->registers.A);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t sre(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = lsr(address, cpu);
    cycles += eor(address, cpu);
    return cycles;
}

nes_cpu_clock_t lsr(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);
    bool newCarryFlag = (value&0x01) != 0;
    value >>= 1;
    cpu->setFlags(CARRY_MASK, newCarryFlag);

    cpu->updateZeroFlag(value);
    cpu->updateNegativeFlag(value);

    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, value);

    return cycles;
}

nes_cpu_clock_t lsra(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->registers.A;
    bool newCarryFlag = (value&0x01) != 0;
    value >>= 1;
    cpu->setFlags(CARRY_MASK, newCarryFlag);

    cpu->updateZeroFlag(value);
    cpu->updateNegativeFlag(value);
    cpu->registers.A = value;

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t pha(uint16_t address, NesCpu * cpu) {
    cpu->pushStackBtye(cpu->registers.A);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t alr(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = myand(address, cpu);
    cycles += lsra(address, cpu);
    return cycles;
}

nes_cpu_clock_t jmp(uint16_t address, NesCpu * cpu) {
    cpu->registers.PC = address;
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t bvc(uint16_t address, NesCpu * cpu) {
    if (!(TEST_OVERFLOW(cpu->registers.P))) {
        return cpu->performBranch(address);
    }
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t cli(uint16_t address, NesCpu * cpu) {
    cpu->setFlags(INTERRUPT_DISABLE_MASK, false);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t rts(uint16_t address, NesCpu * cpu) {
    cpu->registers.PC = cpu->popStackWord() + uint16_t(1);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t adc(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);

    uint8_t carry = 0;
    if (TEST_CARRY(cpu->registers.P)) {
        carry = 1;
    }
    bool newCarryFlag = (int(value) + int(cpu->registers.A) + int(carry)) > 0xFF;
    cpu->setFlags(CARRY_MASK, newCarryFlag);

    int asign = TEST_SIGN_8BIT(cpu->registers.A);
    int valuesign = TEST_SIGN_8BIT(value);

    cpu->registers.A = cpu->registers.A + value + carry;

    cpu->updateZeroFlag(cpu->registers.A);
    cpu->updateNegativeFlag(cpu->registers.A);

    int resultsign = TEST_SIGN_8BIT(cpu->registers.A);

    bool newOverflowFlag = (asign && valuesign && !resultsign) || (!asign && !valuesign && resultsign);
    cpu->setFlags(OVERFLOW_MASK, newOverflowFlag);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t rra(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = ror(address, cpu);
    cycles += adc(address, cpu);
    return cycles;
}

nes_cpu_clock_t ror(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);
    bool newFlagCarry = (value&0x1) != 0;

    value >>= 1;
     if (TEST_CARRY(cpu->registers.P)) {
         value |= 0x80;
     }

     cpu->setFlags(CARRY_MASK, newFlagCarry);
     cpu->updateNegativeFlag(value);
     cpu->updateZeroFlag(value);

    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, value);

     return cycles;
}

nes_cpu_clock_t rora(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->registers.A;
    bool newFlagCarry = (value&0x1) != 0;

    value >>= 1;
    if (TEST_CARRY(cpu->registers.P)) {
        value |= 0x80;
    }

    cpu->setFlags(CARRY_MASK, newFlagCarry);
    cpu->updateNegativeFlag(value);
    cpu->updateZeroFlag(value);

    cpu->registers.A = value;

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t pla(uint16_t address, NesCpu * cpu) {
    cpu->registers.A = cpu->popStackByte();
    cpu->updateZeroFlag(cpu->registers.A);
    cpu->updateNegativeFlag(cpu->registers.A);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t arr(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);

    cpu->registers.A = (cpu->registers.A & value) >> 1;
    if (TEST_CARRY(cpu->registers.P)) {
        cpu->registers.A |= 0x80;
    }

    cpu->updateNegativeFlag(cpu->registers.A);
    cpu->updateZeroFlag(cpu->registers.A);
    bool newCarryFlag = ((cpu->registers.A>>6)&0x1) != 0;
    bool newOverflowFlag = (((cpu->registers.A>>6)^(cpu->registers.A>>5))&0x1) != 0;
    cpu->setFlags(CARRY_MASK, newCarryFlag);
    cpu->setFlags(OVERFLOW_MASK, newOverflowFlag);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t bvs(uint16_t address, NesCpu * cpu) {
    if (TEST_OVERFLOW(cpu->registers.P)) {
       return cpu->performBranch(address);
    }
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t sei(uint16_t address, NesCpu * cpu) {
    cpu->setFlags(INTERRUPT_DISABLE_MASK, true);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t sta(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, cpu->registers.A);

    return cycles;
}

nes_cpu_clock_t aax(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->registers.A & cpu->registers.X;
    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, value);
    return cycles;
}

nes_cpu_clock_t sty(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, cpu->registers.Y);

    return cycles;
}

nes_cpu_clock_t stx(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, cpu->registers.X);

    return cycles;
}

nes_cpu_clock_t dey(uint16_t address, NesCpu * cpu) {
    cpu->registers.Y--;
    cpu->updateNegativeFlag(cpu->registers.Y);
    cpu->updateZeroFlag(cpu->registers.Y);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t txa(uint16_t address, NesCpu * cpu) {
    cpu->registers.A = cpu->registers.X;

    cpu->updateZeroFlag(cpu->registers.A);
    cpu->updateNegativeFlag(cpu->registers.A);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t bcc(uint16_t address, NesCpu * cpu) {
    if (!(TEST_CARRY(cpu->registers.P))) {
        return cpu->performBranch(address);
    }
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t tya(uint16_t address, NesCpu * cpu) {
    cpu->registers.A = cpu->registers.Y;

    cpu->updateNegativeFlag(cpu->registers.A);
    cpu->updateZeroFlag(cpu->registers.A);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t txs(uint16_t address, NesCpu * cpu) {
    cpu->registers.S = cpu->registers.X;

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t ldy(uint16_t address, NesCpu * cpu) {
    cpu->registers.Y = cpu->RAM->read_byte(address);

    cpu->updateZeroFlag(cpu->registers.Y);
    cpu->updateNegativeFlag(cpu->registers.Y);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t lda(uint16_t address, NesCpu * cpu) {
    cpu->registers.A = cpu->RAM->read_byte(address);

    cpu->updateZeroFlag(cpu->registers.A);
    cpu->updateNegativeFlag(cpu->registers.A);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t ldx(uint16_t address, NesCpu * cpu) {
    cpu->registers.X = cpu->RAM->read_byte(address);

    cpu->updateZeroFlag(cpu->registers.X);
    cpu->updateNegativeFlag(cpu->registers.X);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t lax(uint16_t address, NesCpu * cpu) {
    cpu->registers.A = cpu->RAM->read_byte(address);
    cpu->registers.X = cpu->registers.A;
    cpu->updateZeroFlag(cpu->registers.X);
    cpu->updateNegativeFlag(cpu->registers.X);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t tay(uint16_t address, NesCpu * cpu) {
    cpu->registers.Y = cpu->registers.A;

    cpu->updateNegativeFlag(cpu->registers.Y);
    cpu->updateZeroFlag(cpu->registers.Y);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t tax(uint16_t address, NesCpu * cpu) {
    cpu->registers.X = cpu->registers.A;

    cpu->updateNegativeFlag(cpu->registers.X);
    cpu->updateZeroFlag(cpu->registers.X);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t lxa(uint16_t address, NesCpu * cpu) {
    cpu->registers.A = cpu->RAM->read_byte(address);
    cpu->registers.X = cpu->registers.A;
    cpu->updateNegativeFlag(cpu->registers.X);
    cpu->updateZeroFlag(cpu->registers.X);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t bcs(uint16_t address, NesCpu * cpu) {
    if (TEST_CARRY(cpu->registers.P)) {
        return cpu->performBranch(address);
    }
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t clv(uint16_t address, NesCpu * cpu) {
    cpu->setFlags(OVERFLOW_MASK, false);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t tsx(uint16_t address, NesCpu * cpu) {
    cpu->registers.X = cpu->registers.S;

    cpu->updateZeroFlag(cpu->registers.X);
    cpu->updateNegativeFlag(cpu->registers.X);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t cpy(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);

    uint8_t result = cpu->registers.Y - value;
    cpu->updateNegativeFlag(result);
    cpu->updateZeroFlag(result);
    bool newCarryFlag = cpu->registers.Y >= value;
    cpu->setFlags(CARRY_MASK, newCarryFlag);

    return nes_cpu_clock_t(0);

}

nes_cpu_clock_t cmp(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);

    uint8_t result = cpu->registers.A - value;
    cpu->updateNegativeFlag(result);
    cpu->updateZeroFlag(result);
    bool newCarryFlag = cpu->registers.A >= value;
    cpu->setFlags(CARRY_MASK, newCarryFlag);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t dcp(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = dec(address, cpu);
    cycles += cmp(address, cpu);
    return cycles;
}

nes_cpu_clock_t dec(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);
    value--;
    cpu->updateZeroFlag(value);
    cpu->updateNegativeFlag(value);

    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, value);

    return cycles;
}

nes_cpu_clock_t iny(uint16_t address, NesCpu * cpu) {
    cpu->registers.Y++;
    cpu->updateNegativeFlag(cpu->registers.Y);
    cpu->updateZeroFlag(cpu->registers.Y);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t dex(uint16_t address, NesCpu * cpu) {
    cpu->registers.X--;
    cpu->updateNegativeFlag(cpu->registers.X);
    cpu->updateZeroFlag(cpu->registers.X);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t sax(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);
    uint8_t d = cpu->registers.A & cpu->registers.X;

    bool newCarryFlag = (int(d) - int(value)) >= 0;
    cpu->setFlags(CARRY_MASK, newCarryFlag);

    cpu->registers.X = d - value;
    cpu->updateZeroFlag(cpu->registers.X);
    cpu->updateNegativeFlag(cpu->registers.X);
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t bne(uint16_t address, NesCpu * cpu) {
    if (!(TEST_ZERO(cpu->registers.P))) {
        return cpu->performBranch(address);
    }
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t cld(uint16_t address, NesCpu * cpu) {
    cpu->setFlags(DECIMAL_MASK, false);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t cpx(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);

    uint8_t result = cpu->registers.X - value;
    cpu->updateNegativeFlag(result);
    cpu->updateZeroFlag(result);
    bool newCarryFlag = cpu->registers.X >= value;
    cpu->setFlags(CARRY_MASK, newCarryFlag);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t sbc(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);

    uint8_t carry = 0;
    if (!(TEST_CARRY(cpu->registers.P))) {
        carry = 1;
    }
    bool newCarryFlag = ( int(cpu->registers.A) - int(value) - int(carry)) >= 0;
    cpu->setFlags(CARRY_MASK, newCarryFlag);

    int asign = TEST_SIGN_8BIT(cpu->registers.A);
    int valuesign = !(TEST_SIGN_8BIT(value));

    cpu->registers.A = cpu->registers.A - value - carry;

    cpu->updateZeroFlag(cpu->registers.A);
    cpu->updateNegativeFlag(cpu->registers.A);

    int resultsign = TEST_SIGN_8BIT(cpu->registers.A);

    bool newOverflowFlag = (asign && valuesign && !resultsign) || (!asign && !valuesign && resultsign);
    cpu->setFlags(OVERFLOW_MASK, newOverflowFlag);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t isc(uint16_t address, NesCpu * cpu) {
    nes_cpu_clock_t cycles = inc(address, cpu);
    cycles += sbc(address, cpu);
    return cycles;
}

nes_cpu_clock_t inc(uint16_t address, NesCpu * cpu) {
    uint8_t value = cpu->RAM->read_byte(address);
    value++;

    cpu->updateNegativeFlag(value);
    cpu->updateZeroFlag(value);

    nes_cpu_clock_t cycles = cpu->RAM->write_byte(address, value);

    return cycles;
}

nes_cpu_clock_t inx(uint16_t address, NesCpu * cpu) {
    cpu->registers.X++;

    cpu->updateNegativeFlag(cpu->registers.X);
    cpu->updateZeroFlag(cpu->registers.X);

    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t beq(uint16_t address, NesCpu * cpu) {
    if (TEST_ZERO(cpu->registers.P)) {
        return cpu->performBranch(address);
    }
    return nes_cpu_clock_t(0);
}

nes_cpu_clock_t sed(uint16_t address, NesCpu * cpu) {
    cpu->setFlags(DECIMAL_MASK, true);

    return nes_cpu_clock_t(0);
}
