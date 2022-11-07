#pragma once

#include "apu.hpp"
#include "cpu.hpp"
#include "gpu.hpp"
#include "interrupt.hpp"
#include "io.hpp"
#include "memory.hpp"

namespace gbc
{
enum keys_t
{
    DPAD_RIGHT = 0x1,
    DPAD_LEFT = 0x2,
    DPAD_UP = 0x4,
    DPAD_DOWN = 0x8,
    BUTTON_A = 0x10,
    BUTTON_B = 0x20,
    BUTTON_SELECT = 0x40,
    BUTTON_START = 0x80
};

class Machine
{
public:
    Machine(const uint8_t *rom, size_t rom_data_size, bool init = true)
        : Machine(std::string_view{(const char *)rom, rom_data_size}, init) {}
    Machine(const std::string_view rom, bool init = true);
    Machine(const std::vector<uint8_t>& rom, bool init = true)
        : Machine(std::string_view{(const char *)rom.data(), rom.size()}, init) {}

    CPU cpu;
    Memory memory;
    IO io;
    GPU gpu;
    APU apu;

    void simulate();
    void simulate_one_frame();
    void reset();
    uint64_t now() noexcept;
    bool is_running() const noexcept { return this->m_running; }
    bool is_cgb() const noexcept { return this->m_cgb_mode; }

    // set delegates to be notified on interrupts
    enum interrupt
    {
        VBLANK,
        TIMER,
        JOYPAD,
        DEBUG,
    };
    void set_handler(interrupt, interrupt_handler);

    // use keys_t to form an 8-bit mask
    void set_inputs(uint8_t mask);

    // serialization (state-keeping)
    size_t restore_state(const std::vector<uint8_t>&);
    void   serialize_state(std::vector<uint8_t>&) const;

    /// debugging aids ///
    bool verbose_instructions = false;
    bool verbose_interrupts = false;
    bool verbose_banking = false;
    // make the machine stop when an undefined OP happens
    bool stop_when_undefined = false;
    bool break_on_interrupts = false;
    bool break_on_io = false;
    void break_now();
    bool is_breaking() const noexcept;
    void undefined();
    void stop() noexcept;

private:
    bool m_running = true;
    bool m_cgb_mode = false;
};

inline void Machine::simulate() { cpu.simulate(); }
} // namespace gbc
