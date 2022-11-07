#include "cpu.hpp"

#include "instructions.cpp"
#include "machine.hpp"
#include <cassert>

namespace gbc
{
CPU::CPU(Machine& mach) noexcept : m_machine(mach), m_memory(mach.memory) {}

void CPU::reset() noexcept
{
    if (!machine().is_cgb())
    {
        // gameboy DMG initial register values
        registers().af = 0x01b0;
        registers().bc = 0x0013;
        registers().de = 0x00d8;
        registers().hl = 0x014d;
    }
    else
    {
        // gameboy color initial register values
        registers().af = 0x1180;
        registers().bc = 0x0000;
        registers().de = 0xff56;
        registers().hl = 0x000d;
    }
    registers().sp = 0xfffe;
    registers().pc = memory().bootrom_enabled() ? 0x0 : 0x100;
    this->m_state.cycles_total = 0;
}

void CPU::simulate()
{
    // breakpoint handling
    if (UNLIKELY(this->break_time() || !this->m_breakpoints.empty()))
    {
        this->break_checks();
        // user can quit during break
        if (!machine().is_running()) return;
    }
    // handle interrupts
    this->handle_interrupts();

    if (!this->is_halting() && !this->is_stopping()) { this->execute(); }
    else
    {
        // make sure time passes when not executing instructions
        this->hardware_tick();
        // speed switch
        this->handle_speed_switch();
    }
}

void CPU::execute()
{
    // 1. read instruction from memory
    const uint8_t opcode = this->peekop8(0);
    // 2. decode into executable instruction
    auto& instr = decode(opcode);

    // 2a. print the instruction (when enabled)
    if (UNLIKELY(machine().verbose_instructions))
    {
        char prn[128];
        instr.printer(prn, sizeof(prn), *this, opcode);
        printf("%9llu: [pc %04X] opcode %02X: %s\n", gettime(), registers().pc, opcode, prn);
    }

    // 3. increment PC, hardware tick
    registers().pc++;
    this->hardware_tick();

    // 4. run instruction handler
    instr.handler(*this, opcode);

    if (UNLIKELY(machine().verbose_instructions))
    {
        // print out the resulting flags reg
        if (m_state.last_flags != registers().flags)
        {
            m_state.last_flags = registers().flags;
            char fbuf[5];
            printf("* Flags changed: [%s]\n", cstr_flags(fbuf, registers().flags));
        }
    }
    if (UNLIKELY(memory().is_within(registers().pc, Memory::VideoRAM)))
    {
        fprintf(stderr, "ERROR: PC is in the Video RAM area: %04X\n", registers().pc);
        this->break_now();
    }
    if (UNLIKELY(memory().is_within(registers().pc, Memory::EchoRAM)))
    {
        fprintf(stderr, "WARN: PC is in the Echo RAM area: %04X\n", registers().pc);
        this->break_now();
    }
    if (UNLIKELY(memory().is_within(registers().pc, Memory::OAM_RAM)))
    {
        fprintf(stderr, "ERROR: PC is in the OAM RAM area: %04X\n", registers().pc);
        this->break_now();
    }
    if (UNLIKELY(memory().is_within(registers().pc, Memory::IO_Ports)))
    {
        fprintf(stderr, "ERROR: PC is in the I/O port area: %04X\n", registers().pc);
        this->break_now();
    }
}

void CPU::hardware_tick()
{
    this->incr_cycles(4);
    machine().gpu.simulate();
    machine().io.simulate();
    machine().apu.simulate();
}

// it takes 2 instruction-cycles to toggle interrupts
void CPU::enable_interrupts() noexcept
{
    if (this->m_state.intr_pending <= 0) { this->m_state.intr_pending = 2; }
}
void CPU::disable_interrupts() noexcept { this->m_state.intr_pending = -2; }

void CPU::handle_interrupts()
{
    // enable/disable interrupts over cycles
    if (UNLIKELY(m_state.intr_pending != 0))
    {
        if (m_state.intr_pending > 0)
        {
            m_state.intr_pending--;
            if (!m_state.intr_pending) this->m_state.ime = true;
        }
        else if (m_state.intr_pending < 0)
        {
            m_state.intr_pending++;
            if (!m_state.intr_pending) this->m_state.ime = false;
        }
    }
    // check if interrupts are enabled and pending
    const uint8_t imask = machine().io.interrupt_mask();
    if (UNLIKELY(this->ime() && imask != 0x0))
    {
        // disable interrupts immediately
        this->m_state.ime = false;
        this->m_state.asleep = false;
        // execute pending interrupts (sorted by priority)
        auto& io = machine().io;
        if (imask & 0x1)
            this->interrupt(io.vblank);
        else if (imask & 0x2)
            this->interrupt(io.lcd_stat);
        else if (imask & 0x4)
            this->interrupt(io.timerint);
        else if (imask & 0x8)
            this->interrupt(io.serialint);
        else if (imask & 0x10)
            this->interrupt(io.joypadint);
    }
    else if (UNLIKELY(this->m_state.haltbug && imask != 0))
    {
        // do *NOT* call interrupt handler when buggy HALTing
        this->m_state.asleep = false;
        this->m_state.haltbug = false;
    }
}
void CPU::interrupt(interrupt_t& intr)
{
    if (UNLIKELY(machine().verbose_interrupts))
    { printf("%9llu: Executing interrupt %s (%#x)\n", this->gettime(), intr.name, intr.mask); }
    // disable interrupt request
    machine().io.reg(IO::REG_IF) &= ~intr.mask;
    // set interrupt bit
    // this->m_state.reg_ie |= intr.mask;
    this->hardware_tick();
    this->hardware_tick();
    // push PC and jump to INTR addr
    this->push_and_jump(intr.fixed_address);
    // sometimes we want to break on interrupts
    if (UNLIKELY(machine().break_on_interrupts && !machine().is_breaking()))
    { machine().break_now(); }
    if (intr.callback) intr.callback(machine(), intr);
}

instruction_t& CPU::decode(const uint8_t opcode)
{
    switch (opcode)
    {
    case 0x00: // NOP
        return instr_NOP;
    case 0x08: // LD SP, imm16
        return instr_LD_N_SP;
    case 0x10: // STOP
        return instr_STOP;
    case 0x76: // HALT
        return instr_HALT;
    // LD D, imm8
    case 0x06:
    case 0x16:
    case 0x26:
    case 0x36:
    case 0x0E:
    case 0x1E:
    case 0x2E:
    case 0x3E:
        return instr_LD_D_N;
    // LD B, D
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
    // LD C, D
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
    // LD D, D
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57:
    // LD E, D
    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B:
    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x5F:
    // LD H, D
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    // LD L, D
    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B:
    case 0x6C:
    case 0x6D:
    case 0x6E:
    case 0x6F:
    // LD (HL), D
    // NOTE: 0x76: is HALT and *not* LD (HL), (HL)
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x77:
    // LD A, D
    case 0x78:
    case 0x79:
    case 0x7A:
    case 0x7B:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
        return instr_LD_D_D;
    // LD A, R
    case 0x02:
    case 0x12:
    case 0x0A:
    case 0x1A:
        return instr_LD_R_A_R;
    case 0xEA: // LD (imm16), A
    case 0xFA: // LD A, (imm16)
        return instr_LD_N_A_N;
    case 0x22: // LDI (HL), A
    case 0x32: // LDD (HL), A
    case 0x2A: // LDI A, (HL)
    case 0x3A: // LDD A, (HL)
        return instr_LDID_HL_A;
    case 0xE2: // LD (FF00+C), A
    case 0xF2: // LD A, (FF00+C)
    case 0xE0: // LD (FF00+imm8), A
    case 0xF0: // LD A, (FF00+imm8)
        return instr_LD_FF00_A;
    // LD R, imm16
    case 0x01:
    case 0x11:
    case 0x21:
    case 0x31:
        return instr_LD_R_N;
    case 0xE8:
        return instr_ADD_SP_N;
    case 0xF8: // LD HL, SP+imm8
    case 0xF9: // LD SP, HL
        return instr_LD_HL_SP;
    // POP R
    case 0xC1:
    case 0xD1:
    case 0xE1:
    case 0xF1:
    // PUSH R
    case 0xC5:
    case 0xD5:
    case 0xE5:
    case 0xF5:
        return instr_PUSH_POP;
    // ALU operations
    // ADD A, D
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
    case 0x87:
        return instr_ALU_A_D;
    // ADC A, D
    case 0x88:
    case 0x89:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x8F:
        return instr_ALU_A_D;
    // SUB A, D
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x96:
    case 0x97:
        return instr_ALU_A_D;
    // SBC A, D
    case 0x98:
    case 0x99:
    case 0x9A:
    case 0x9B:
    case 0x9C:
    case 0x9D:
    case 0x9E:
    case 0x9F:
        return instr_ALU_A_D;
    // AND A, D
    case 0xA0:
    case 0xA1:
    case 0xA2:
    case 0xA3:
    case 0xA4:
    case 0xA5:
    case 0xA6:
    case 0xA7:
        return instr_ALU_A_D;
    // XOR A, D
    case 0xA8:
    case 0xA9:
    case 0xAA:
    case 0xAB:
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
        return instr_ALU_A_D;
    // OR  A, D
    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB7:
        return instr_ALU_A_D;
    // CP A, D
    case 0xB8:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
        return instr_ALU_A_D;
    // ALU OP A, im8
    case 0xC6:
    case 0xCE:
    case 0xD6:
    case 0xDE:
    case 0xE6:
    case 0xEE:
    case 0xF6:
    case 0xFE:
        return instr_ALU_A_N;
    // INC R, DEC R
    case 0x03:
    case 0x0B:
    case 0x13:
    case 0x1B:
    case 0x23:
    case 0x2B:
    case 0x33:
    case 0x3B:
        return instr_INC_DEC_R;
    // INC D, DEC D
    case 0x04:
    case 0x05:
    case 0x0C:
    case 0x0D:
    case 0x14:
    case 0x15:
    case 0x1C:
    case 0x1D:
    case 0x24:
    case 0x25:
    case 0x2C:
    case 0x2D:
    case 0x34:
    case 0x35:
    case 0x3C:
    case 0x3D:
        return instr_INC_DEC_D;
    // ADD HL, R
    case 0x09:
    case 0x19:
    case 0x29:
    case 0x39:
        return instr_ADD_HL_R;
    case 0x27: // DA A
        return instr_DAA;
    case 0x2F: // CPL A
        return instr_CPL_A;
    case 0x37: // SCF
    case 0x3F: // CCF
        return instr_SCF_CCF;
    case 0x07: // RLC A
    case 0x17: // RL A
    case 0x0F: // RRC A
    case 0x1F: // RR A
        return instr_RLC_RRC;
    case 0xC3: // JP imm16
    case 0xC2: // JP nz, imm16
    case 0xCA: // JP z, imm16
    case 0xD2: // JP nc, imm16
    case 0xDA: // JP c, imm16
        return instr_JP;
    case 0xE9: // JP HL
        return instr_JP_HL;
    case 0x18: // JR imm8
    case 0x20: // JR nz, imm8
    case 0x28: // JR z, imm8
    case 0x30: // JR nc, imm8
    case 0x38: // JR c, imm8
        return instr_JR_N;
    case 0xCD: // CALL imm16
    case 0xC4: // CALL nz, imm16
    case 0xCC: // CALL z, imm16
    case 0xD4: // CALL nc, imm16
    case 0xDC: // CALL c, imm16
        return instr_CALL;
    case 0xC9: // RET
    case 0xC0: // RET nz
    case 0xC8: // RET z
    case 0xD0: // RET nc
    case 0xD8: // RET c
        return instr_RET;
    case 0xD9: // RETI
        return instr_RETI;
    // RST 0x0, 0x08, 0x10, 0x18
    case 0xC7:
    case 0xCF:
    case 0xD7:
    case 0xDF:
    // RST 0x20, 0x028, 0x30, 0x38
    case 0xE7:
    case 0xEF:
    case 0xF7:
    case 0xFF:
        return instr_RST;
    case 0xF3: // DI
    case 0xFB: // EI
        return instr_DI_EI;
    case 0xCB:
        return instr_CB_EXT;
    }
    return instr_MISSING;
}

uint8_t CPU::peekop8(int disp) { return memory().read8(registers().pc + disp); }
uint16_t CPU::peekop16(int disp) { return memory().read16(registers().pc + disp); }
uint8_t CPU::readop8()
{
    const uint8_t operand = peekop8(0);
    registers().pc++;
    hardware_tick();
    return operand;
}
uint16_t CPU::readop16()
{
    const uint16_t operand = peekop16(0);
    registers().pc += 2;
    hardware_tick();
    hardware_tick();
    return operand;
}

uint8_t CPU::read_hl() { return this->mtread8(registers().hl); }
void CPU::write_hl(const uint8_t value) { this->mtwrite8(registers().hl, value); }
uint8_t CPU::mtread8(uint16_t addr)
{
    const uint8_t value = memory().read8(addr);
    this->hardware_tick();
    return value;
}
void CPU::mtwrite8(uint16_t addr, uint8_t value)
{
    memory().write8(addr, value);
    this->hardware_tick();
}
uint16_t CPU::mtread16(uint16_t addr)
{
    const uint16_t value = memory().read16(addr);
    this->hardware_tick();
    this->hardware_tick();
    return value;
}
void CPU::mtwrite16(uint16_t addr, uint16_t value)
{
    memory().write16(addr, value);
    this->hardware_tick();
    this->hardware_tick();
}

void CPU::incr_cycles(int count)
{
    assert(count >= 0);
    this->m_state.cycles_total += count;
}

void CPU::stop()
{
    this->m_state.stopped = true;
    // preparing a speed switch?
    if (machine().io.reg(IO::REG_KEY1) & 0x1) { this->m_state.switch_cycles = 4; }
    // disable screen etc.
    machine().io.perform_stop();
}
void CPU::handle_speed_switch()
{
    if (UNLIKELY(this->m_state.switch_cycles > 0))
    {
        this->m_state.switch_cycles--;
        if (this->m_state.switch_cycles == 0)
        {
            // stop the stopping
            this->m_state.stopped = false;
            // change speed
            memory().do_switch_speed();
            // this can turn the LCD back on
            machine().io.deactivate_stop();
        }
    }
}

void CPU::wait()
{
    this->m_state.asleep = true;
    this->m_state.haltbug = false;
}
void CPU::buggy_halt()
{
    this->m_state.asleep = true;
    this->m_state.haltbug = true;
}

void CPU::jump(const uint16_t dest)
{
    if (UNLIKELY(machine().verbose_instructions))
    { printf("* Jumped to %04X (from %04X)\n", dest, registers().pc); }
    this->registers().pc = dest;
}
void CPU::push_value(uint16_t address)
{
    this->hardware_tick();
    registers().sp -= 2;
    this->mtwrite16(registers().sp, address);
}
void CPU::push_and_jump(uint16_t address)
{
    this->push_value(registers().pc);
    this->jump(address);
}

int CPU::restore_state(const std::vector<uint8_t>& data, int off)
{
    this->m_state = *(state_t*) &data.at(off);
    return sizeof(m_state);
}
void CPU::serialize_state(std::vector<uint8_t>& res) const
{
    res.insert(res.end(), (uint8_t*) &m_state, (uint8_t*) &m_state + sizeof(m_state));
}
} // namespace gbc
