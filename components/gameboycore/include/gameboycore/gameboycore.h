/**
    \file gameboycore.h
    \brief Encapsulate Gameboy hardware
    \author Natesh Narain <nnaraindev@gmail.com>
*/

#ifndef GAMEBOYCORE_H
#define GAMEBOYCORE_H

#include "gameboycore/gameboycore_api.h"

#include "gameboycore/cpu.h"
#include "gameboycore/mmu.h"
#include "gameboycore/gpu.h"
#include "gameboycore/apu.h"
#include "gameboycore/joypad.h"
#include "gameboycore/link.h"

#include <cstdint>
#include <vector>
#include <string>

//! GameboyCore namespace
namespace gb
{
    /**
        \brief Encapsulation for Gameboy emulation
        \class GameboyCore
        \ingroup API
    */
    class GAMEBOYCORE_API GameboyCore
    {
    public:
        enum class ColorTheme
        {
            DEFAULT,
            GOLD,
            GREEN
        };

        GameboyCore();
        GameboyCore(const GameboyCore&) = delete;
        ~GameboyCore();

        /**
            runs `steps` number of steps on the gameboycore
        */
        void update(int steps = 1);

        /**
            Run emulation for a single frame
        */
        void emulateFrame();

        /**
            Load a ROM file
        */
        void open(const std::string& filename);

        /**
            Load byte buffer into virtual memroy
        */
        void loadROM(const std::vector<uint8_t>& buffer);

        /**
            Load byte buffer into virtual memory
        */
        void loadROM(const uint8_t* rom, uint32_t size);

        /**
            Reset the GameboyCore state
        */
        void reset();

        /**
            Enable debug output
        */
        void setDebugMode(bool debug);

        /**
            Set Color theme
        */
        void setColorTheme(ColorTheme theme);

        /**
         * Read memory
        */
        uint8_t readMemory(uint16_t addr);

        /**
         * Write memory
        */
        void writeMemory(uint16_t addr, uint8_t value);
        
        /**
            Set scanline callback
        */
        void setScanlineCallback(GPU::RenderScanlineCallback callback);

        /**
            Set VBlank callback
        */
        void setVBlankCallback(GPU::VBlankCallback callback);

        /**
            Set audio sample callback
        */
        void setAudioSampleCallback(APU::AudioSampleCallback callback);
        
        /**
            Joypad key input event
        */
        void input(Joy::Key key, bool pressed);

        /**
            Get battery RAM

            This copies the battery backed RAM from the emulator and returns it to the user
        */
        std::vector<uint8_t> getBatteryRam() const;
        
        /**
            Set battery RAM
        */
        void setBatteryRam(const std::vector<uint8_t>& ram);

        /**
            Write a byte to the serial port
        */
        void linkWrite(uint8_t byte);

        /**
            Set Link ready callback

            Set a callback that fires when the core is ready to transfer a byte to the serial port
        */
        void setLinkReadyCallback(Link::ReadyCallback callback);

        /**
            Serialize GameboyCore state
        */
        std::vector<uint8_t> serialize() const;

        /**
            Deserialize GameboyCore state
        */
        void deserialize(const std::vector<uint8_t>& data);

        /**
            Set the time to be read from the RTC register (MBC3)
        */
        void setTimeProvider(const TimeProvider provider);

        /**
            Set instruction callback
        */
        void setInstructionCallback(std::function<void(const gb::Instruction&, const uint16_t addr)> instr);

        CPU::Ptr& getCPU();
        MMU::Ptr& getMMU();
        GPU::Ptr& getGPU();
        APU::Ptr& getAPU();
        Joy::Ptr& getJoypad();
        Link::Ptr& getLink();

        bool isDone() const;

    private:
        class Impl;
        Impl* impl_;
    };
}

#endif // GAMEBOYCORE_H
