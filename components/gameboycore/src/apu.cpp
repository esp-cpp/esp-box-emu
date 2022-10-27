
/**
    \author Natesh Narain <nnaraindev@gmail.com>
*/

#include "gameboycore/apu.h"
#include "gameboycore/memorymap.h"
#include "gameboycore/detail/audio/square_wave_channel.h"
#include "gameboycore/detail/audio/wave_channel.h"
#include "gameboycore/detail/audio/noise_channel.h"

#include "bitutil.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace gb
{
    /* Private Interface */

    class APU::Impl
    {
        //! Cycles for 512 Hz with ~4.2 MHz clock
        static constexpr unsigned int CYCLES_512HZ = 8192;
        //! APU down sampling rate (CPU clock / sample rate of host system)
        static constexpr unsigned int DOWNSAMPLE_RATE = 4200000 / 44100;
        //! Starting address of the APU registers
        static constexpr uint16_t APU_REG_BASE = memorymap::NR10_REGISTER;

    public:
        explicit Impl(MMU::Ptr& mmu) :
            mmu_(mmu),
            square1_(true),
            square2_(false),
            frame_sequencer_counter_(CYCLES_512HZ),
            frame_sequencer_(0),
            down_sample_counter_(0)
        {
            // intercept all read/write attempts here
            for (uint16_t i = memorymap::NR10_REGISTER; i <= memorymap::WAVE_PATTERN_RAM_END; ++i)
            {
                mmu->addReadHandler(i, std::bind(&Impl::read, this, std::placeholders::_1));
                mmu->addWriteHandler(i, std::bind(&Impl::write, this, std::placeholders::_1, std::placeholders::_2));
            }

            // init register memory
            std::fill(apu_registers.begin(), apu_registers.end(), (uint8_t)0);

            // set extra read bits
            initExtraBits();
        }

        /**
            update with cycles
        */
        void update(uint8_t cycles)
        {
            // ignore if apu is disabled
            if (!isEnabled()) return;

            while (cycles--)
            {
                // frame sequencer clock
                if (frame_sequencer_counter_-- <= 0)
                {
                    frame_sequencer_counter_ = CYCLES_512HZ;

                    clockFrameSequencer();
                }

                // run channel logic
                square1_.step();
                square2_.step();
                wave_.step();
                noise_.step();

                // down sampling is required since the APU can generate audio at a rate faster than the host system will play
                if (--down_sample_counter_ == 0)
                {
                    down_sample_counter_ = DOWNSAMPLE_RATE;

                    // generate left and right audio samples
                    mixVolumes();
                }
            }
        }

        uint8_t getSound1Volume() const noexcept
        {
            return square1_.getVolume();
        }

        uint8_t getSound2Volume() const noexcept
        {
            return square2_.getVolume();
        }

        uint8_t getSound3Volume() const noexcept
        {
            return wave_.getVolume();
        }

        uint8_t getSound4Volume() const noexcept
        {
            return noise_.getVolume();
        }

        void setAudioSampleCallback(AudioSampleCallback callback)
        {
            send_audio_sample_ = callback;
        }

    private:

        void clockFrameSequencer()
        {
            switch (frame_sequencer_)
            {
            case 0:
            case 2:
                clockLength();
                square1_.clockSweep();
                break;
            case 4:
                clockLength();
                break;
            case 6:
                clockLength();
                square1_.clockSweep();
                break;
            case 7:
                clockVolume();
                break;
            }

            frame_sequencer_++;

            if (frame_sequencer_ >= 8)
            {
                frame_sequencer_ = 0;
            }
        }

        void mixVolumes()
        {
            static constexpr float AMPLITUDE = 30000;

            // convert sound output between [0, 1]
            const auto sound1 = (float)square1_.getVolume() / 15.f;
            const auto sound2 = (float)square2_.getVolume() / 15.f;
            const auto sound3 = (float)wave_.getVolume() / 15.f;
            const auto sound4 = (float)noise_.getVolume() / 15.f;

            float left_sample = 0;
            float right_sample = 0;

            // add left channel contributions
            if (channel_left_enabled_[0])
                left_sample += sound1;
            if (channel_left_enabled_[1])
                left_sample += sound2;
            if (channel_left_enabled_[2])
                left_sample += sound3;
            if (channel_left_enabled_[3])
                left_sample += sound4;

            // add right channel contributions
            if (channel_right_enabled_[0])
                right_sample += sound1;
            if (channel_right_enabled_[1])
                right_sample += sound2;
            if (channel_right_enabled_[2])
                right_sample += sound3;
            if (channel_right_enabled_[3])
                right_sample += sound4;

            // average the totals
            left_sample /= 4.0f;
            right_sample /= 4.0f;

            // volume per channel between [0, 1]
            const auto right_volume = ((float)right_volume_) / 7.f;
            const auto left_volume = ((float)left_volume_) / 7.f;

            // generate a sample
            const auto left = (int16_t)(left_sample * left_volume * AMPLITUDE);
            const auto right = (int16_t)(right_sample * right_volume * AMPLITUDE);

            // send the samples to the host system
            if (send_audio_sample_)
                send_audio_sample_(left, right);
        }

        void clockLength() noexcept
        {
            square1_.clockLength();
            square2_.clockLength();
            wave_.clockLength();
            noise_.clockLength();
        }

        void clockVolume() noexcept
        {
            square1_.clockVolume();
            square2_.clockVolume();
            noise_.clockVolume();
        }

        bool isEnabled() const noexcept
        {
            return isBitSet(apuRead(memorymap::NR52_REGISTER), 7) != 0;
        }

        uint8_t read(uint16_t addr)
        {
            uint8_t value = 0;

            const auto& extras = extra_bits_[addr - APU_REG_BASE];

            if (addr == memorymap::NR52_REGISTER)
            {
                value = apuRead(addr) & 0xF0;

                value |= square1_.isEnabled() << 0;
                value |= square2_.isEnabled() << 1;
                value |= wave_.isEnabled() << 2;
                value |= noise_.isEnabled() << 3;
            }
            else
            {
                if (addr >= memorymap::NR10_REGISTER && addr <= memorymap::NR14_REGISTER)
                {
                    value = square1_.read(addr - memorymap::NR10_REGISTER);
                }
                else if (addr >= memorymap::NR20_REGISTER && addr <= memorymap::NR24_REGISTER)
                {
                    value = square2_.read(addr - memorymap::NR20_REGISTER);
                }
                else if (addr >= memorymap::NR30_REGISTER && addr <= memorymap::NR34_REGISTER)
                {
                    value = wave_.read(addr - memorymap::NR30_REGISTER);
                }
                else if (addr >= memorymap::WAVE_PATTERN_RAM_START && addr <= memorymap::WAVE_PATTERN_RAM_END)
                {
                    value = wave_.readWaveRam(addr);
                }
                else if (addr >= memorymap::NR41_REGISTER && addr <= memorymap::NR44_REGISTER)
                {
                    value = noise_.read(addr - memorymap::NR41_REGISTER);
                }
                else if (addr >= memorymap::NR50_REGISTER && addr <= memorymap::NR52_REGISTER)
                {
                    value = apuRead(addr);
                }
            }

            return value | extras;
        }

        void write(uint8_t value, uint16_t addr)
        {
            if (addr == memorymap::NR52_REGISTER)
            {
                // check if APU is being disabled
                if (isClear(value, 0x80))
                {
                    clearRegisters();

                    square1_.disable();
                    square2_.disable();
                    wave_.disable();
                    noise_.disable();

                    frame_sequencer_ = 0;
                }

                // check is being enabled
                if (!isEnabled() && isSet(value, 0x80))
                {
                    frame_sequencer_counter_ = CYCLES_512HZ;
                }

                apuWrite(value, addr);
            }
            else if (addr == memorymap::NR50_REGISTER && isEnabled())
            {
                right_volume_ = value & 0x07;
                right_enabled_ = (value & 0x08) != 0;

                left_volume_ = (value & 0x70) >> 4;
                left_enabled_ = (value & 0x80) != 0;

                apuWrite(value, addr);
            }
            else if (addr == memorymap::NR51_REGISTER && isEnabled())
            {
                channel_right_enabled_[0] = (value & 0x01) != 0;
                channel_right_enabled_[1] = (value & 0x02) != 0;
                channel_right_enabled_[2] = (value & 0x04) != 0;
                channel_right_enabled_[3] = (value & 0x08) != 0;
                channel_left_enabled_[0] = (value & 0x10) != 0;
                channel_left_enabled_[1] = (value & 0x20) != 0;
                channel_left_enabled_[2] = (value & 0x40) != 0;
                channel_left_enabled_[3] = (value & 0x80) != 0;

                apuWrite(value, addr);
            }
            else
            {
                if (isEnabled())
                {
                    if (addr >= memorymap::NR10_REGISTER && addr <= memorymap::NR14_REGISTER)
                    {
                        square1_.write(value, addr - memorymap::NR10_REGISTER);
                    }
                    else if (addr >= memorymap::NR20_REGISTER && addr <= memorymap::NR24_REGISTER)
                    {
                        square2_.write(value, addr - memorymap::NR20_REGISTER);
                    }
                    else if (addr >= memorymap::NR30_REGISTER && addr <= memorymap::NR34_REGISTER)
                    {
                        wave_.write(value, addr - memorymap::NR30_REGISTER);
                    }
                    else if (addr >= memorymap::WAVE_PATTERN_RAM_START && addr <= memorymap::WAVE_PATTERN_RAM_END)
                    {
                        wave_.writeWaveRam(value, addr);
                    }
                    else if (addr >= memorymap::NR41_REGISTER && addr <= memorymap::NR44_REGISTER)
                    {
                        noise_.write(value, addr - memorymap::NR41_REGISTER);
                    }
                }
            }
        }

        uint8_t apuRead(uint16_t addr) const noexcept
        {
            return apu_registers[addr - APU_REG_BASE];
        }

        void apuWrite(uint8_t value, uint16_t addr) noexcept
        {
            apu_registers[addr - APU_REG_BASE] = value;
        }

        void clearRegisters()
        {
            for (auto addr = APU_REG_BASE; addr < memorymap::WAVE_PATTERN_RAM_START; ++addr)
            {
                if (addr == memorymap::NR52_REGISTER) continue;
                mmu_->write((uint8_t)0, addr);
            }
        }

        void initExtraBits() noexcept
        {
            // NR10 - NR14
            extra_bits_[0x00] = 0x80;
            extra_bits_[0x01] = 0x3F;
            extra_bits_[0x02] = 0x00;
            extra_bits_[0x03] = 0xFF;
            extra_bits_[0x04] = 0xBF;

            // NR20 - NR24
            extra_bits_[0x05] = 0xFF;
            extra_bits_[0x06] = 0x3F;
            extra_bits_[0x07] = 0x00;
            extra_bits_[0x08] = 0xFF;
            extra_bits_[0x09] = 0xBF;

            // NR30 - NR34
            extra_bits_[0x0A] = 0x7F;
            extra_bits_[0x0B] = 0xFF;
            extra_bits_[0x0C] = 0x9F;
            extra_bits_[0x0D] = 0xFF;
            extra_bits_[0x0E] = 0xBF;

            // NR40 - NR44
            extra_bits_[0x0F] = 0xFF;
            extra_bits_[0x10] = 0xFF;
            extra_bits_[0x11] = 0x00;
            extra_bits_[0x12] = 0x00;
            extra_bits_[0x13] = 0xBF;

            // NR50 - NR52
            extra_bits_[0x14] = 0x00;
            extra_bits_[0x15] = 0x00;
            extra_bits_[0x16] = 0x70;

            //
            extra_bits_[0x17] = 0xFF;
            extra_bits_[0x18] = 0xFF;
            extra_bits_[0x19] = 0xFF;
            extra_bits_[0x1A] = 0xFF;
            extra_bits_[0x1B] = 0xFF;
            extra_bits_[0x1C] = 0xFF;
            extra_bits_[0x1D] = 0xFF;
            extra_bits_[0x1E] = 0xFF;
            extra_bits_[0x1F] = 0xFF;

            // wave ram
            extra_bits_[0x20] = 0x00;
            extra_bits_[0x21] = 0x00;
            extra_bits_[0x22] = 0x00;
            extra_bits_[0x23] = 0x00;
            extra_bits_[0x24] = 0x00;
            extra_bits_[0x25] = 0x00;
            extra_bits_[0x26] = 0x00;
            extra_bits_[0x27] = 0x00;
            extra_bits_[0x28] = 0x00;
            extra_bits_[0x29] = 0x00;
            extra_bits_[0x2A] = 0x00;
            extra_bits_[0x2B] = 0x00;
            extra_bits_[0x2C] = 0x00;
            extra_bits_[0x2D] = 0x00;
            extra_bits_[0x2E] = 0x00;
            extra_bits_[0x2F] = 0x00;
        }

    private:
        MMU::Ptr& mmu_;

        //! Sound 1 - Square wave with Sweep
        detail::SquareWaveChannel square1_;
        //! Sound 2 - Square wave
        detail::SquareWaveChannel square2_;
        //! Sound 3 - Wave from wave ram
        detail::WaveChannel wave_;
        //! Sound 4 - Noise Channel
        detail::NoiseChannel noise_;

        //! callback to host when an audio sample is computed
        AudioSampleCallback send_audio_sample_;

        //! APU cycle counter
        int frame_sequencer_counter_;

        //! APU internal timer
        int frame_sequencer_;

        //! APU registers
        std::array<uint8_t, 0x30> apu_registers;

        //! left volume
        uint8_t left_volume_;
        //! left enabled
        bool left_enabled_;
        //! right volume
        uint8_t right_volume_;
        //! right enabled
        bool right_enabled_;

        //! left channel enables
        bool channel_left_enabled_[4];
        //! right channel enables
        bool channel_right_enabled_[4];

        //!
        uint8_t down_sample_counter_;

        //! bits that are ORed into the value when read
        std::array<uint8_t, 0x30> extra_bits_;
    };

    /* Public Interface */

    APU::APU(MMU::Ptr& mmu) :
        impl_(new Impl(mmu))
    {
    }

    void APU::update(uint8_t cycles)
    {
        impl_->update(cycles);
    }
    
    uint8_t APU::getSound1Volume()
    {
        return impl_->getSound1Volume();
    }

    uint8_t APU::getSound2Volume()
    {
        return impl_->getSound2Volume();
    }

    uint8_t APU::getSound3Volume()
    {
        return impl_->getSound3Volume();
    }

    uint8_t APU::getSound4Volume()
    {
        return impl_->getSound4Volume();
    }

    void APU::setAudioSampleCallback(AudioSampleCallback callback)
    {
        impl_->setAudioSampleCallback(callback);
    }

    APU::~APU()
    {
        delete impl_;
    }
}