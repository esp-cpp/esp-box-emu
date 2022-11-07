// only include this file once!
#include "machine.hpp"
#include "printers.hpp"
#define DEF_INSTR(x)                                                                               \
    static instruction_t instr_##x { handler_##x, printer_##x }
#define INSTRUCTION(x) static void handler_##x
#define PRINTER(x) static int printer_##x
union imm8_t
{
    uint8_t u8;
    int8_t s8;
};

namespace gbc
{
INSTRUCTION(NOP)(CPU&, const uint8_t)
{
    // NOP takes 4 T-states (instruction decoding)
}
PRINTER(NOP)(char* buffer, size_t len, CPU&, const uint8_t) { return snprintf(buffer, len, "NOP"); }

INSTRUCTION(LD_N_SP)(CPU& cpu, const uint8_t) { cpu.mtwrite16(cpu.readop16(), cpu.registers().sp); }
PRINTER(LD_N_SP)(char* buffer, size_t len, CPU& cpu, const uint8_t)
{
    return snprintf(buffer, len, "LD (%04X), SP", cpu.peekop16(1));
}

INSTRUCTION(LD_R_N)(CPU& cpu, const uint8_t opcode)
{
    cpu.registers().getreg_sp(opcode) = cpu.readop16();
}
PRINTER(LD_R_N)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    return snprintf(buffer, len, "LD %s, %04x", cstr_reg(opcode, true), cpu.peekop16(1));
}

INSTRUCTION(ADD_HL_R)(CPU& cpu, const uint8_t opcode)
{
    auto& reg = cpu.registers().getreg_sp(opcode);
    auto& hl = cpu.registers().hl;
    auto& flags = cpu.registers().flags;
    setflag(false, flags, MASK_NEGATIVE);
    setflag(((hl & 0x0fff) + (reg & 0x0fff)) & 0x1000, flags, MASK_HALFCARRY);
    setflag(((hl & 0x0ffff) + (reg & 0x0ffff)) & 0x10000, flags, MASK_CARRY);
    hl += reg;
    cpu.hardware_tick();
}
PRINTER(ADD_HL_R)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    return snprintf(buffer, len, "ADD HL, %s", cstr_reg(opcode, true));
}

INSTRUCTION(LD_R_A_R)(CPU& cpu, const uint8_t opcode)
{
    if (opcode & 0x8) { cpu.registers().accum = cpu.mtread8(cpu.registers().getreg_sp(opcode)); }
    else
    {
        cpu.mtwrite8(cpu.registers().getreg_sp(opcode), cpu.registers().accum);
    }
}
PRINTER(LD_R_A_R)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    if (opcode & 0x8) { return snprintf(buffer, len, "LD A, (%s)", cstr_reg(opcode, true)); }
    return snprintf(buffer, len, "LD (%s), A", cstr_reg(opcode, true));
}

INSTRUCTION(INC_DEC_R)(CPU& cpu, const uint8_t opcode)
{
    auto& reg = cpu.registers().getreg_sp(opcode);
    if ((opcode & 0x8) == 0) { reg++; }
    else
    {
        reg--;
    }
    cpu.hardware_tick();
}
PRINTER(INC_DEC_R)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    if ((opcode & 0x8) == 0) { return snprintf(buffer, len, "INC %s", cstr_reg(opcode, true)); }
    return snprintf(buffer, len, "DEC %s", cstr_reg(opcode, true));
}

INSTRUCTION(INC_DEC_D)(CPU& cpu, const uint8_t opcode)
{
    const uint8_t dst = opcode >> 3;
    uint8_t value;
    if (dst != 0x6)
    {
        if ((opcode & 0x1) == 0) { cpu.registers().getdest(dst)++; }
        else
        {
            cpu.registers().getdest(dst)--;
        }
        value = cpu.registers().getdest(dst);
    }
    else
    {
        value = cpu.read_hl();
        if ((opcode & 0x1) == 0) { value++; }
        else
        {
            value--;
        }
        cpu.write_hl(value);
    }
    auto& flags = cpu.registers().flags;
    setflag(opcode & 0x1, flags, MASK_NEGATIVE);
    setflag(value == 0, flags, MASK_ZERO); // set zero
    if ((opcode & 0x1) == 0)
        setflag((value & 0xF) == 0x0, flags, MASK_HALFCARRY);
    else
        setflag((value & 0xF) == 0xF, flags, MASK_HALFCARRY);
}
PRINTER(INC_DEC_D)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    const char* mnemonic = (opcode & 0x1) ? "DEC" : "INC";
    return snprintf(buffer, len, "%s %s", mnemonic, cstr_dest(opcode >> 3));
}

INSTRUCTION(LD_D_N)(CPU& cpu, const uint8_t opcode)
{
    const uint8_t imm8 = cpu.readop8();
    if (((opcode >> 3) & 0x7) != 0x6) { cpu.registers().getdest(opcode >> 3) = imm8; }
    else
    {
        cpu.write_hl(imm8);
    }
}
PRINTER(LD_D_N)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    if (((opcode >> 3) & 0x7) != 0x6)
        return snprintf(buffer, len, "LD %s, %02X", cstr_dest(opcode >> 3), cpu.peekop8(1));
    else
        return snprintf(buffer, len, "LD (HL=%04X), %02X", cpu.registers().hl, cpu.peekop8(1));
}

INSTRUCTION(RLC_RRC)(CPU& cpu, const uint8_t opcode)
{
    auto& accum = cpu.registers().accum;
    auto& flags = cpu.registers().flags;
    switch (opcode)
    {
    case 0x07:
    {
        // RLCA, rotate A left
        const uint8_t bit7 = accum & 0x80;
        accum = (accum << 1) | (bit7 >> 7);
        flags = 0;
        setflag(bit7, flags, MASK_CARRY); // old bit7 to CF
    }
    break;
    case 0x0F:
    {
        // RRCA, rotate A right
        const uint8_t bit0 = accum & 0x1;
        accum = (accum >> 1) | (bit0 << 7);
        flags = 0;
        setflag(bit0, flags, MASK_CARRY); // old bit0 to CF
    }
    break;
    case 0x17:
    {
        // RLA, rotate A left, old CF to bit 0
        const uint8_t bit7 = accum & 0x80;
        accum = (accum << 1) | ((flags & MASK_CARRY) >> 4);
        flags = 0;
        setflag(bit7, flags, MASK_CARRY); // old bit7 to CF
    }
    break;
    case 0x1F:
    {
        // RRA, rotate A right, old CF to bit 7
        const uint8_t bit0 = accum & 0x1;
        accum = (accum >> 1) | ((flags & MASK_CARRY) << 3);
        flags = 0;
        setflag(bit0, flags, MASK_CARRY); // old bit0 to CF
    }
    break;
    default:
        GBC_ASSERT(0 && "Unknown opcode in RLC/RRC handler");
    }
}
PRINTER(RLC_RRC)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    const char* mnemonic[4] = {"RLC", "RRC", "RL", "RR"};
    return snprintf(buffer, len, "%s A (A = %02X)", mnemonic[opcode >> 3], cpu.registers().accum);
}

INSTRUCTION(LD_D_D)(CPU& cpu, const uint8_t opcode)
{
    const bool HL = (opcode & 0x7) == 0x6;
    uint8_t reg;
    if (!HL)
        reg = cpu.registers().getdest(opcode);
    else
        reg = cpu.read_hl();

    if (((opcode >> 3) & 0x7) != 0x6) { cpu.registers().getdest(opcode >> 3) = reg; }
    else
    {
        cpu.write_hl(reg);
    }
}
PRINTER(LD_D_D)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    return snprintf(buffer, len, "LD %s, %s", cstr_dest(opcode >> 3), cstr_dest(opcode >> 0));
}

INSTRUCTION(LD_N_A_N)(CPU& cpu, const uint8_t opcode)
{
    const uint16_t addr = cpu.readop16();
    if (opcode == 0xEA)
    {
        // load into (N) from A
        cpu.mtwrite8(addr, cpu.registers().accum);
    }
    else
    {
        // load into A from (N)
        cpu.registers().accum = cpu.mtread8(addr);
    }
    cpu.hardware_tick(); // 16 T-states
}
PRINTER(LD_N_A_N)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    if (opcode == 0xEA)
        return snprintf(buffer, len, "LD (%04X), A (A = %02X)", cpu.peekop16(1),
                        cpu.registers().accum);
    else
        return snprintf(buffer, len, "LD A, (%04X)", cpu.peekop16(1));
}

INSTRUCTION(LDID_HL_A)(CPU& cpu, const uint8_t opcode)
{
    if ((opcode & 0x8) == 0)
    {
        // load from A into (HL)
        cpu.write_hl(cpu.registers().accum);
    }
    else
    {
        // load from (HL) into A
        cpu.registers().accum = cpu.read_hl();
    }
    if ((opcode & 0x10) == 0) { cpu.registers().hl++; }
    else
    {
        cpu.registers().hl--;
    }
}
PRINTER(LDID_HL_A)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    const char* mnemonic = (opcode & 0x10) ? "LDD" : "LDI";
    if ((opcode & 0x8) == 0)
        return snprintf(buffer, len, "%s (HL=%04X), A", mnemonic, cpu.registers().hl);
    else
        return snprintf(buffer, len, "%s A, (HL=%04X)", mnemonic, cpu.registers().hl);
}

INSTRUCTION(DAA)(CPU& cpu, const uint8_t)
{
    auto& regs = cpu.registers();
    if (regs.flags & MASK_NEGATIVE)
    {
        if (regs.flags & MASK_CARRY) regs.accum -= 0x60;
        if (regs.flags & MASK_HALFCARRY) regs.accum -= 0x06;
    }
    else
    {
        if ((regs.flags & MASK_CARRY) || regs.accum > 0x99)
        {
            regs.accum += 0x60;
            setflag(true, regs.flags, MASK_CARRY);
        }
        if ((regs.flags & MASK_HALFCARRY) || (regs.accum & 0xF) > 0x9) { regs.accum += 0x06; }
    }
    setflag(regs.accum == 0, regs.flags, MASK_ZERO);
    setflag(false, regs.flags, MASK_HALFCARRY);
}
PRINTER(DAA)(char* buffer, size_t len, CPU&, uint8_t) { return snprintf(buffer, len, "DAA"); }

INSTRUCTION(CPL_A)(CPU& cpu, const uint8_t)
{
    cpu.registers().accum = ~cpu.registers().accum;
    auto& regs = cpu.registers();
    setflag(true, regs.flags, MASK_NEGATIVE);
    setflag(true, regs.flags, MASK_HALFCARRY);
}
PRINTER(CPL_A)(char* buffer, size_t len, CPU&, uint8_t) { return snprintf(buffer, len, "CPL A"); }

INSTRUCTION(SCF_CCF)(CPU& cpu, const uint8_t opcode)
{
    auto& flags = cpu.registers().flags;
    if ((opcode & 0x8) == 0)
    {
        // Set CF
        flags |= MASK_CARRY;
    }
    else
    {
        // Complement CF
        setflag(not(flags & MASK_CARRY), flags, MASK_CARRY);
    }
    setflag(false, flags, MASK_NEGATIVE);
    setflag(false, flags, MASK_HALFCARRY);
}
PRINTER(SCF_CCF)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    return snprintf(buffer, len, (opcode & 0x8) ? "CCF (CY=0)" : "SCF (CY=1)");
}

// ALU A, D / A, N
INSTRUCTION(ALU_A_D)(CPU& cpu, const uint8_t opcode)
{
    const uint8_t alu_op = (opcode >> 3) & 0x7;
    // <alu> A, D
    if ((opcode & 0x7) != 0x6) { cpu.registers().alu(alu_op, cpu.registers().getdest(opcode)); }
    else
    {
        cpu.registers().alu(alu_op, cpu.read_hl());
    }
}
PRINTER(ALU_A_D)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    return snprintf(buffer, len, "%s A, %s", cstr_alu(opcode >> 3), cstr_dest(opcode));
}

INSTRUCTION(ALU_A_N)(CPU& cpu, const uint8_t opcode)
{
    const uint8_t alu_op = (opcode >> 3) & 0x7;
    // <alu> A, N
    const uint8_t imm8 = cpu.readop8();
    cpu.registers().alu(alu_op, imm8);
}
PRINTER(ALU_A_N)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    return snprintf(buffer, len, "%s A, 0x%02x", cstr_alu(opcode >> 3), cpu.peekop8(1));
}

INSTRUCTION(JP)(CPU& cpu, const uint8_t opcode)
{
    const uint16_t dest = cpu.readop16();
    if ((opcode & 1) || (cpu.registers().compare_flags(opcode)))
    {
        cpu.jump(dest);
        cpu.hardware_tick();
    }
}
PRINTER(JP)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    if (opcode & 1) { return snprintf(buffer, len, "JP 0x%04x", cpu.peekop16(1)); }
    char temp[128];
    fill_flag_buffer(temp, sizeof(temp), opcode, cpu.registers().flags);
    return snprintf(buffer, len, "JP 0x%04x (%s)", cpu.peekop16(1), temp);
}

INSTRUCTION(PUSH_POP)(CPU& cpu, const uint8_t opcode)
{
    if (opcode & 4)
    {
        // PUSH R
        cpu.push_value(cpu.registers().getreg_af(opcode));
    }
    else
    {
        // POP R
        cpu.registers().getreg_af(opcode) = cpu.mtread16(cpu.registers().sp);
        cpu.registers().sp += 2;
        if (((opcode >> 4) & 0x3) == 0x3)
        {
            // NOTE: POP AF requires clearing flag bits 0-3
            cpu.registers().flags &= 0xF0;
        }
    }
}
PRINTER(PUSH_POP)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    if (opcode & 4)
    {
        return snprintf(buffer, len, "PUSH %s (0x%04x)", cstr_reg(opcode, false),
                        cpu.registers().getreg_af(opcode));
    }
    return snprintf(buffer, len, "POP %s (0x%04x)", cstr_reg(opcode, false),
                    cpu.memory().read16(cpu.registers().sp));
}

INSTRUCTION(RET)(CPU& cpu, const uint8_t opcode)
{
    if ((opcode & 0xef) == 0xc9 || cpu.registers().compare_flags(opcode))
    {
        cpu.registers().pc = cpu.mtread16(cpu.registers().sp);
        cpu.registers().sp += 2;
        if (UNLIKELY(cpu.machine().verbose_instructions))
        { printf("* Returned to 0x%04x\n", cpu.registers().pc); }
        if (opcode != 0xc9)
        {
            cpu.hardware_tick(); // RET nzc needs one more tick
        }
    }
    cpu.hardware_tick();
}
PRINTER(RET)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    if (opcode == 0xc9) { return snprintf(buffer, len, "RET"); }
    char temp[128];
    fill_flag_buffer(temp, sizeof(temp), opcode, cpu.registers().flags);
    return snprintf(buffer, len, "RET %s", temp);
}

INSTRUCTION(RETI)(CPU& cpu, const uint8_t)
{
    cpu.registers().pc = cpu.mtread16(cpu.registers().sp);
    cpu.registers().sp += 2;
    if (UNLIKELY(cpu.machine().verbose_instructions))
    { printf("* Returned (w/interrupts) to 0x%04x\n", cpu.registers().pc); }
    cpu.hardware_tick();
    cpu.enable_interrupts();
}
PRINTER(RETI)(char* buffer, size_t len, CPU&, uint8_t) { return snprintf(buffer, len, "RETI"); }

INSTRUCTION(RST)(CPU& cpu, const uint8_t opcode)
{
    const uint16_t dst = opcode & 0x38;
    if (UNLIKELY(cpu.registers().pc == dst + 1))
    {
        printf(">>> RST loop detected at vector 0x%04x\n", dst);
        cpu.break_now();
        return;
    }
    // jump to vector area
    cpu.push_and_jump(dst);
}
PRINTER(RST)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    return snprintf(buffer, len, "RST 0x%02x", opcode & 0x38);
}

INSTRUCTION(STOP)(CPU& cpu, const uint8_t)
{
    // STOP is a weirdo two-byte instruction
    cpu.registers().pc++;
    if (cpu.machine().io.joypad_is_disabled())
    {
        printf("The machine has stopped with joypad disabled\n");
        cpu.break_now();
    }
    // enter stopped state
    cpu.stop();
}
PRINTER(STOP)(char* buffer, size_t len, CPU&, uint8_t) { return snprintf(buffer, len, "STOP"); }

INSTRUCTION(JR_N)(CPU& cpu, const uint8_t opcode)
{
    const imm8_t disp{.u8 = cpu.readop8()};
    cpu.hardware_tick();
    if (opcode == 0x18 || (cpu.registers().compare_flags(opcode)))
    { cpu.jump(cpu.registers().pc + disp.s8); }
}
PRINTER(JR_N)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    const imm8_t disp{.u8 = cpu.peekop8(1)};
    const uint16_t dest = cpu.registers().pc + 2 + disp.s8;
    if (opcode & 0x20)
    {
        char temp[128];
        fill_flag_buffer(temp, sizeof(temp), opcode, cpu.registers().flags);
        return snprintf(buffer, len, "JR %+hhd (%s) => %04X", disp.s8, temp, dest);
    }
    return snprintf(buffer, len, "JR %+hhd => %04X", disp.s8, dest);
}

INSTRUCTION(HALT)(CPU& cpu, const uint8_t)
{
    if (cpu.ime()) { cpu.wait(); }
    else
    {
        cpu.buggy_halt();
    }
}
PRINTER(HALT)(char* buffer, size_t len, CPU&, uint8_t) { return snprintf(buffer, len, "HALT"); }

INSTRUCTION(CALL)(CPU& cpu, const uint8_t opcode)
{
    const uint16_t dest = cpu.readop16();
    if ((opcode & 1) || cpu.registers().compare_flags(opcode))
    {
        // push address of **next** instr
        cpu.push_and_jump(dest);
    }
}
PRINTER(CALL)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    if (opcode & 1) { return snprintf(buffer, len, "CALL %04X", cpu.peekop16(1)); }
    char temp[128];
    fill_flag_buffer(temp, sizeof(temp), opcode, cpu.registers().flags);
    return snprintf(buffer, len, "CALL %04X (%s)", cpu.peekop16(1), temp);
}

INSTRUCTION(ADD_SP_N)(CPU& cpu, const uint8_t)
{
    const imm8_t imm{.u8 = cpu.readop8()};
    auto& regs = cpu.registers();
    const int calc = (regs.sp + imm.s8) & 0xFFFF;
    regs.flags = 0;
    setflag(((regs.sp ^ imm.s8 ^ calc) & 0x100) == 0x100, regs.flags, MASK_CARRY);
    setflag(((regs.sp ^ imm.s8 ^ calc) & 0x10) == 0x10, regs.flags, MASK_HALFCARRY);
    cpu.registers().sp = calc;
    cpu.hardware_tick();
    cpu.hardware_tick();
}
PRINTER(ADD_SP_N)(char* buffer, size_t len, CPU& cpu, uint8_t)
{
    return snprintf(buffer, len, "ADD SP, 0x%02x", cpu.peekop8(1));
}

INSTRUCTION(LD_FF00_A)(CPU& cpu, const uint8_t opcode)
{
    switch (opcode)
    {
    case 0xE2:
        cpu.mtwrite8(0xFF00 + cpu.registers().c, cpu.registers().accum);
        return;
    case 0xF2:
        cpu.registers().accum = cpu.mtread8(0xFF00 + cpu.registers().c);
        return;
    case 0xE0:
        cpu.mtwrite8(0xFF00 + cpu.readop8(), cpu.registers().accum);
        return;
    case 0xF0:
        cpu.registers().accum = cpu.mtread8(0xFF00 + cpu.readop8());
        return;
    }
    GBC_ASSERT(0);
}
PRINTER(LD_FF00_A)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    switch (opcode)
    {
    case 0xE2:
        return snprintf(buffer, len, "LD (FF00+C=%02X), A", cpu.registers().c);
    case 0xF2:
        return snprintf(buffer, len, "LD A, (FF00+C=%02X)", cpu.registers().c);
    case 0xE0:
        return snprintf(buffer, len, "LD (FF00+%02X), A", cpu.peekop8(1));
    case 0xF0:
        return snprintf(buffer, len, "LD A, (FF00+%02X)", cpu.peekop8(1));
    }
    GBC_ASSERT(0);
}

INSTRUCTION(LD_HL_SP)(CPU& cpu, const uint8_t opcode)
{
    if (opcode == 0xF8)
    {
        // the ADD operation is signed
        const imm8_t imm{.u8 = cpu.readop8()};
        cpu.registers().flags = 0;
        setflag(((cpu.registers().sp & 0xf) + (imm.u8 & 0x0f)) & 0x10, cpu.registers().flags,
                MASK_HALFCARRY);
        setflag(((cpu.registers().sp & 0xff) + (imm.u8 & 0xff)) & 0x100, cpu.registers().flags,
                MASK_CARRY);
        cpu.registers().hl = cpu.registers().sp + imm.s8;
    }
    else
    {
        cpu.registers().sp = cpu.registers().hl;
    }
    cpu.hardware_tick();
}
PRINTER(LD_HL_SP)(char* buffer, size_t len, CPU& cpu, uint8_t opcode)
{
    if (opcode == 0xF8) { return snprintf(buffer, len, "LD HL, SP + %02X", cpu.peekop8(1)); }
    return snprintf(buffer, len, "LD SP, HL (HL=%04X)", cpu.registers().hl);
}

INSTRUCTION(JP_HL)(CPU& cpu, const uint8_t) { cpu.jump(cpu.registers().hl); }
PRINTER(JP_HL)(char* buffer, size_t len, CPU& cpu, uint8_t)
{
    return snprintf(buffer, len, "JP HL (HL=%04X)", cpu.registers().hl);
}

INSTRUCTION(DI_EI)(CPU& cpu, const uint8_t opcode)
{
    if (opcode & 0x08) { cpu.enable_interrupts(); }
    else
    {
        cpu.disable_interrupts();
    }
}
PRINTER(DI_EI)(char* buffer, size_t len, CPU&, uint8_t opcode)
{
    const char* mnemonic = (opcode & 0x08) ? "EI" : "DI";
    return snprintf(buffer, len, "%s", mnemonic);
}

INSTRUCTION(CB_EXT)(CPU& cpu, const uint8_t)
{
    const uint8_t opcode = cpu.readop8();
    const bool HL = (opcode & 0x7) == 0x6;
    uint8_t reg;
    if (!HL)
        reg = cpu.registers().getdest(opcode);
    else
        reg = cpu.read_hl();

    // BIT, RESET, SET
    if (opcode >> 6)
    {
        const uint8_t bit = (opcode >> 3) & 7;
        switch (opcode >> 6)
        {
        case 0x1:
        { // BIT
            const int set = reg & (1 << bit);
            // set flags
            cpu.registers().flags &= ~MASK_NEGATIVE;
            cpu.registers().flags |= MASK_HALFCARRY;
            setflag(set == 0, cpu.registers().flags, MASK_ZERO);
            // BIT only takes 8/12 T-cycles
            return;
        }
        case 0x2: // RESET
            reg &= ~(1 << bit);
            break;
        case 0x3: // SET
            reg |= 1 << bit;
            break;
        }
    }
    else if ((opcode & 0xF0) == 0x00)
    {
        auto& flags = cpu.registers().flags;
        flags = 0;
        if (opcode & 0x8)
        {
            // RRC D, rotate D right, keep old bit0
            setflag(reg & 0x1, flags, MASK_CARRY); // old bit0 to CF
            reg = (reg >> 1) | (reg << 7);
        }
        else
        {
            // RLC D, rotate D left, keep old bit7
            setflag(reg & 0x80, flags, MASK_CARRY); // old bit7 to CF
            reg = (reg << 1) | (reg >> 7);
        }
        setflag(reg == 0, cpu.registers().flags, MASK_ZERO);
    }
    else if ((opcode & 0xF0) == 0x10)
    {
        auto& flags = cpu.registers().flags;
        // NOTE: dont reset flags here
        if (opcode & 0x8)
        {
            // RR D, rotate D right through carry, old CF to bit 7
            const uint8_t bit0 = reg & 0x1;
            reg = (reg >> 1) | ((flags & MASK_CARRY) << 3);
            flags = 0;
            setflag(bit0, flags, MASK_CARRY); // old bit0 to CF
        }
        else
        {
            // RL D, rotate D left through carry, old CF to bit 0
            const uint8_t bit7 = reg & 0x80;
            reg = (reg << 1) | ((flags & MASK_CARRY) >> 4);
            flags = 0;
            setflag(bit7, flags, MASK_CARRY); // old bit7 to CF
        }
        setflag(reg == 0, flags, MASK_ZERO);
    }
    else if ((opcode & 0xF0) == 0x20)
    {
        auto& flags = cpu.registers().flags;
        flags = 0;
        if (opcode & 0x8)
        {
            // SRA D
            setflag(reg & 0x1, flags, MASK_CARRY);
            reg >>= 1;
            reg |= (reg & 0x40) << 1;
        }
        else
        {
            // SLA D
            setflag(reg & 0x80, flags, MASK_CARRY);
            reg <<= 1;
        }
        setflag(reg == 0, cpu.registers().flags, MASK_ZERO);
    }
    else if ((opcode & 0xf0) == 0x30)
    {
        if ((opcode & 0x8) == 0x0)
        {
            // SWAP D
            reg = (reg >> 4) | (reg << 4);
            cpu.registers().flags = 0;
            setflag(reg == 0, cpu.registers().flags, MASK_ZERO);
        }
        else
        {
            // SRL D (logical)
            cpu.registers().flags = 0;
            setflag(reg & 0x1, cpu.registers().flags, MASK_CARRY);
            reg >>= 1;
            setflag(reg == 0, cpu.registers().flags, MASK_ZERO);
        }
    }
    else
    {
        fprintf(stderr, "Missing instruction: %#x\n", opcode);
        GBC_ASSERT(0 && "Unimplemented extended instruction");
    }
    // all instructions on this opcode go into the same dest
    if (!HL)
        cpu.registers().getdest(opcode) = reg;
    else
        cpu.write_hl(reg);
}
PRINTER(CB_EXT)(char* buffer, size_t len, CPU& cpu, uint8_t)
{
    const uint8_t opcode = cpu.peekop8(1);
    if (opcode >> 6)
    {
        const char* mnemonic[] = {"IMPLEMENT ME", "BIT", "RES", "SET"};
        return snprintf(buffer, len, "%s %u, %s", mnemonic[opcode >> 6], (opcode >> 3) & 0x7,
                        cstr_dest(opcode));
    }
    else
    {
        const char* mnemonic[] = {"RLC", "RRC", "RL", "RR", "SLA", "SRA", "SWAP", "SRL"};
        return snprintf(buffer, len, "%s %s", mnemonic[opcode >> 3], cstr_dest(opcode));
    }
}

INSTRUCTION(MISSING)(CPU& cpu, const uint8_t opcode)
{
    fprintf(stderr, "Missing instruction: %#x\n", opcode);
    // pause for each instruction
    cpu.print_and_pause(cpu, opcode);
}
PRINTER(MISSING)(char* buffer, size_t len, CPU&, const uint8_t opcode)
{
    return snprintf(buffer, len, "MISSING opcode 0x%02x", opcode);
}

DEF_INSTR(NOP);
DEF_INSTR(LD_N_SP);
DEF_INSTR(LD_R_N);
DEF_INSTR(ADD_HL_R);
DEF_INSTR(LD_R_A_R);
DEF_INSTR(INC_DEC_R);
DEF_INSTR(INC_DEC_D);
DEF_INSTR(LD_D_N);
DEF_INSTR(LD_D_D);
DEF_INSTR(LD_N_A_N);
DEF_INSTR(JR_N);
DEF_INSTR(RLC_RRC);
DEF_INSTR(LDID_HL_A);
DEF_INSTR(DAA);
DEF_INSTR(CPL_A);
DEF_INSTR(SCF_CCF);
DEF_INSTR(HALT);
DEF_INSTR(ALU_A_D);
DEF_INSTR(ALU_A_N);
DEF_INSTR(PUSH_POP);
DEF_INSTR(RST);
DEF_INSTR(RET);
DEF_INSTR(RETI);
DEF_INSTR(STOP);
DEF_INSTR(JP);
DEF_INSTR(CALL);
DEF_INSTR(ADD_SP_N);
DEF_INSTR(LD_FF00_A);
DEF_INSTR(LD_HL_SP);
DEF_INSTR(JP_HL);
DEF_INSTR(DI_EI);
DEF_INSTR(CB_EXT);
DEF_INSTR(MISSING);
} // namespace gbc
