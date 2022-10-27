
/**
    \author Natesh Narain <nnaraindev@gmail.com>
*/

#ifndef GAMEBOY_WAVE_CHANNEL_H
#define GAMEBOY_WAVE_CHANNEL_H

#include <cstdint>

namespace gb
{
    namespace detail
    {
        /**
            \class Wave
            \brief Wave register
            \ingroup Audio
        */
        class WaveChannel
        {
        public:
            static const int LENGTH_MASK = 0xFF;

        public:
            WaveChannel() :
                dac_enabled_(false),
                length_load_(0),
                volume_code_(0),
                frequency_(0),
                length_enabled_(false),
                trigger_(false),
                wave_ram_{0},
                timer_load_(0),
                timer_(0),
                sample_index_(0),
                length_counter_(0),
                volume_(0),
                shift_table_{{4, 0, 1, 2}},
                is_enabled_(false)
            {
            }

            ~WaveChannel()
            {
            }

            void clockLength()
            {
                // wave channel length counter will disable channel when it gets to zero
                if (length_enabled_ && length_counter_ > 0)
                {
                    if (length_counter_-- <= 0)
                    {
                        is_enabled_ = false;
                    }
                }
            }

            void step()
            {
                // wave channel internal timer
                if (timer_-- <= 0)
                {
                    timer_ = timer_load_;

                    // next sample index
                    sample_index_ = (sample_index_ + 1) % wave_ram_.size();

                    // get the next volume 
                    volume_ = wave_ram_[sample_index_];
                    volume_ >>= shift_table_[volume_code_];
                }

                if (!dac_enabled_ || !is_enabled_)
                    volume_ = 0;
            }

            uint8_t read(uint16_t register_number)
            {
                switch (register_number)
                {
                case 0:
                    return (dac_enabled_ << 7);
                case 1:
                    return length_load_;
                case 2: 
                    return volume_code_ << 5;
                case 3:
                    return frequency_ & 0x00FF;
                case 4:
                    return (trigger_ << 7) | (length_enabled_ << 6) | ((frequency_ & 0x0700) >> 8);
                default:
                    return 0;
                }
            }

            void write(uint8_t value, uint16_t register_number)
            {
                switch (register_number)
                {
                case 0:
                    dac_enabled_ = (value & 0x80) != 0;
                    break;
    
                case 1:
                    length_load_ = value;
                    break;

                case 2:
                    volume_code_ = (value & 0x60) >> 5;
                    break;

                case 3:
                    frequency_ = (frequency_ & 0xFF00) | value;
                    break;

                case 4:
                    frequency_ = (frequency_ & 0x00FF) | ((value & 0x0007) << 8);
                    length_enabled_ = (value & 0x40) != 0;
                    trigger_ = (value & 0x80) != 0;

                    if (trigger_)
                        trigger();

                    break;
                }
            }

            uint8_t readWaveRam(uint16_t addr)
            {
                auto idx = (addr & 0x000F) * 2;
                return ((wave_ram_[idx]) << 4) | wave_ram_[idx+1];
            }

            void writeWaveRam(uint8_t value, uint16_t addr)
            {
                auto idx = (addr & 0x0F) * 2;

                wave_ram_[idx] = (value & 0xF0) >> 4;
                wave_ram_[idx+1] = value & 0x0F;
            }

            bool isEnabled() const
            {
                return is_enabled_;
            }

            uint8_t getVolume() const
            {
                return volume_;
            }

            void disable()
            {
                is_enabled_ = false;
            }

        private:

            void trigger()
            {
                is_enabled_ = true;

                timer_load_ = (2048 - frequency_) * 2;
                timer_ = timer_load_;

                length_counter_ = length_load_;
            }

            bool dac_enabled_;
            uint8_t length_load_;
            uint8_t volume_code_;
            uint16_t frequency_;
            bool length_enabled_;
            bool trigger_;

            std::array<uint8_t, 32> wave_ram_;
            

            int timer_load_;
            int timer_;
            int sample_index_;
            int length_counter_;
            uint8_t volume_;

            std::array<uint8_t, 4> shift_table_;

            bool is_enabled_;
        };
    }
}

#endif // GAMEBOYCORE_WAVE_H
