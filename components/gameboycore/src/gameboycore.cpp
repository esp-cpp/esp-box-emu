
#include "gameboycore/gameboycore.h"
#include "gameboycore/cartinfo.h"
#include "bitutil.h"

#include <fstream>
#include <memory>
#include <exception>

namespace gb
{
    /* Private Interface */

    class GameboyCore::Impl
    {
    public:
        CPU::Ptr  cpu;
        MMU::Ptr  mmu;
        GPU::Ptr  gpu;
        APU::Ptr  apu;
        Joy::Ptr  joy;
        Link::Ptr link;

        GPU::RenderScanlineCallback scanline_callback_;
        GPU::VBlankCallback vblank_callback_;
        APU::AudioSampleCallback audio_sample_callback_;

        void setScanlineCallback(GPU::RenderScanlineCallback fn)
        {
            scanline_callback_ = fn;

            if (gpu)
                gpu->setRenderCallback(scanline_callback_);
        }

        void setVBlankCallback(GPU::VBlankCallback fn)
        {
            vblank_callback_ = fn;

            if (gpu)
                gpu->setVBlankCallback(vblank_callback_);
        }

        void setAudioSampleCallback(APU::AudioSampleCallback fn)
        {
            audio_sample_callback_ = fn;

            if (apu)
                apu->setAudioSampleCallback(audio_sample_callback_);
        }

        void loadROM(const uint8_t* rom, uint32_t size)
        {
            mmu.reset(new MMU(rom, size));

            gpu.reset(new GPU(mmu));
            apu.reset(new APU(mmu));
            link.reset(new Link(mmu));

            cpu.reset(new CPU(mmu, gpu, apu, link));

            joy.reset(new Joy(*mmu));

            // setup callback
            gpu->setRenderCallback(scanline_callback_);
            gpu->setVBlankCallback(vblank_callback_);
            apu->setAudioSampleCallback(audio_sample_callback_);

            setColorTheme(ColorTheme::GOLD);
        }

        std::vector<uint8_t> serialize() const
        {
            const auto cpu_data = cpu->serialize();
            
            const auto ram_start = mmu->getptr(memorymap::SWITCHABLE_ROM_BANK_END + 1);
            const auto ram_end = mmu->getptr(memorymap::INTERRUPT_ENABLE);

            const auto serialize_size = cpu_data.size() + static_cast<size_t>(ram_end - ram_start);
            
            std::vector<uint8_t> data;
            data.reserve(serialize_size);

            std::copy(cpu_data.begin(), cpu_data.end(), std::back_inserter(data));
            std::copy(ram_start, ram_end, std::back_inserter(data));

            return data;
        }

        void deserialize(const std::vector<uint8_t>& data)
        {
            std::array<uint8_t, 12> cpu_state;
            std::copy(data.begin(), data.begin() + cpu_state.size(), cpu_state.begin());

            const auto ram_start = mmu->getptr(memorymap::SWITCHABLE_ROM_BANK_END + 1);
            std::copy(data.begin() + cpu_state.size(), data.end(), ram_start);
        }

        void setTimeProvider(const TimeProvider provider)
        {
            mmu->setTimeProvider(provider);
        }

        void setInstructionCallback(std::function<void(const Instruction&, const uint16_t addr)> fn)
        {
            cpu->setInstructionCallback(fn);
        }

        void setColorTheme(ColorTheme theme)
        {
            switch (theme)
            {
            case gb::GameboyCore::ColorTheme::DEFAULT:
                gpu->setPaletteColor(255, 255, 255, 0);
                gpu->setPaletteColor(196, 196, 196, 1);
                gpu->setPaletteColor( 96,  96,  96, 2);
                gpu->setPaletteColor(  0,   0,   0, 3);
                break;
            case gb::GameboyCore::ColorTheme::GOLD:
                gpu->setPaletteColor(252, 232, 140, 0);
                gpu->setPaletteColor(220, 180,  92, 1);
                gpu->setPaletteColor(152, 124,  60, 2);
                gpu->setPaletteColor(76,   60,  28, 3);
                break;
            case gb::GameboyCore::ColorTheme::GREEN:
                gpu->setPaletteColor(155, 188, 15, 0);
                gpu->setPaletteColor(139, 172, 15, 1);
                gpu->setPaletteColor( 48,  98, 48, 2);
                gpu->setPaletteColor( 15,  56, 15, 3);
                break;
            default:
                break;
            }
        }
    };

    /* Public Interface */

    GameboyCore::GameboyCore() :
        impl_(new Impl)
    {
    }

    GameboyCore::~GameboyCore()
    {
        delete impl_;
    }

    void GameboyCore::update(int steps)
    {
        while(steps--)
        {
            impl_->cpu->step();
        }
    }

    void GameboyCore::emulateFrame()
    {
        auto& cpu = impl_->cpu;
        auto& ly = impl_->mmu->get(memorymap::LY_REGISTER);

        // Run to starting point of line=0
        while (ly != 0)
        {
            cpu->step();
        }
        
        // run for a frames worth of scanlines line=144
        while (ly <= 143)
        {
            cpu->step();
        }
    }

    void GameboyCore::open(const std::string& filename)
    {
        std::ifstream file{ filename, std::ios::ate | std::ios::binary };
        if (!file.is_open())
        {
            throw std::runtime_error(std::string("Could not load ROM file: " + filename));
        }

        auto size = (std::size_t)file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer;
        buffer.resize(size);

        file.read((char*)buffer.data(), buffer.size());

        loadROM(buffer);
    }

    void GameboyCore::loadROM(const std::vector<uint8_t>& buffer)
    {
        loadROM(buffer.data(), buffer.size());
    }

    void GameboyCore::loadROM(const uint8_t* rom, uint32_t size)
    {
        impl_->loadROM(rom, size);
    }

    void GameboyCore::reset()
    {
        impl_->cpu->reset();
    }

    void GameboyCore::setDebugMode(bool debug)
    {
        impl_->cpu->setDebugMode(debug);
    }

    void GameboyCore::setColorTheme(ColorTheme theme)
    {
        impl_->setColorTheme(theme);
    }

    uint8_t GameboyCore::readMemory(uint16_t addr)
    {
        return impl_->mmu->read(addr);
    }

    void GameboyCore::writeMemory(uint16_t addr, uint8_t value)
    {
        impl_->mmu->write(value, addr);
    }

    void GameboyCore::setScanlineCallback(GPU::RenderScanlineCallback callback)
    {
        impl_->setScanlineCallback(callback);
    }

    void GameboyCore::setVBlankCallback(GPU::VBlankCallback callback)
    {
        impl_->setVBlankCallback(callback);
    }

    void GameboyCore::setAudioSampleCallback(APU::AudioSampleCallback callback)
    {
        impl_->setAudioSampleCallback(callback);
    }

    void GameboyCore::input(Joy::Key key, bool pressed)
    {
        if (pressed)
            impl_->joy->press(key);
        else
            impl_->joy->release(key);
    }

    std::vector<uint8_t> GameboyCore::getBatteryRam() const
    {
        return impl_->mmu->getBatteryRam();
    }

    void GameboyCore::setBatteryRam(const std::vector<uint8_t>& ram)
    {
        impl_->mmu->setBatteryRam(ram);
    }

    void GameboyCore::linkWrite(uint8_t byte)
    {
        impl_->link->recieve(byte);
    }

    void GameboyCore::setLinkReadyCallback(Link::ReadyCallback callback)
    {
        impl_->link->setReadyCallback(callback);
    }

    std::vector<uint8_t> GameboyCore::serialize() const
    {
        return impl_->serialize();
    }

    void GameboyCore::deserialize(const std::vector<uint8_t>& data)
    {
        impl_->deserialize(data);
    }

    void GameboyCore::setTimeProvider(const TimeProvider provider)
    {
        impl_->setTimeProvider(provider);
    }

    void GameboyCore::setInstructionCallback(std::function<void(const gb::Instruction&, const uint16_t addr)> fn)
    {
        impl_->setInstructionCallback(fn);
    }

    CPU::Ptr& GameboyCore::getCPU()
    {
        return impl_->cpu;
    }

    MMU::Ptr& GameboyCore::getMMU()
    {
        return impl_->mmu;
    }

    GPU::Ptr& GameboyCore::getGPU()
    {
        return impl_->gpu;
    }

    APU::Ptr& GameboyCore::getAPU()
    {
        return impl_->apu;
    }

    Joy::Ptr& GameboyCore::getJoypad()
    {
        return impl_->joy;
    }

    Link::Ptr& GameboyCore::getLink()
    {
        return impl_->link;
    }

    bool GameboyCore::isDone() const
    {
        return impl_->cpu->isHalted();
    }
}
