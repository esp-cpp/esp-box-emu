/**
    \author Natesh Narain <nnaraindev@gmail.com>
    \date   Nov 3 2016
*/

#ifndef GAMEBOYCORE_NOISE_H
#define GAMEBOYCORE_NOISE_H

#include <cstdint>
#include <array>

namespace gb
{
    namespace detail
    {
        /**
            \class NoiseChannel
            \brief Generate white noise
            \ingroup Audio
        */
        class NoiseChannel
        {
        public:
            NoiseChannel() :
                length_(0),
                length_envelope_(0),
                envelope_add_mode_(false),
                envelope_default_(0),
                division_ratio_(0),
                width_mode_(false),
                shift_clock_frequency_(0),
                length_enabled_(false),
                trigger_(false),
                volume_(0),
                output_volume_(0),
                length_counter_(0),
                timer_load_(0),
                timer_(0),
                divisor_table_{{8, 16, 32, 48, 64, 80, 96, 112}},
                lfsr_(0),
                dac_enabled_(false),
                is_enabled_(false)
            {
            }

            void step()
            {
                // count down timer
                if (--timer_ <= 0)
                {
                    // reload timer
                    timer_ = timer_load_;

                    // It has a 15 - bit shift register with feedback.When clocked by the frequency timer, the low two bits(0 and 1) are XORed, 
                    // all bits are shifted right by one, and the result of the XOR is put into the now - empty high bit. If width mode is 1 (NR43), 
                    // the XOR result is ALSO put into bit 6 AFTER the shift, resulting in a 7 - bit LFSR.
                    // The waveform output is bit 0 of the LFSR, INVERTED.
                    uint8_t xored = (lfsr_ & 0x01) ^ ((lfsr_ >> 1) & 0x01);
                    lfsr_ >>= 1;
                    lfsr_ |= (xored << 14);

                    if (width_mode_)
                    {
                        lfsr_ = (lfsr_ & 0x04) | (xored << 6);
                    }

                    if (is_enabled_ && dac_enabled_ && (lfsr_ & 0x01) == 0)
                    {
                        output_volume_ = volume_;
                    }
                    else
                    {
                        output_volume_ = 0;
                    }
                }
            }

            void clockLength()
            {
                if (length_enabled_ && length_counter_ > 0)
                {
                    if (--length_counter_ <= 0)
                    {
                        is_enabled_ = false;
                    }
                }
            }

            void clockVolume()
            {
                // count down envelope timer
                if (envelope_timer_-- <= 0)
                {
                    // reload envelope timer
                    envelope_timer_ = length_envelope_;

                    if (envelope_add_mode_ && volume_ < 15)
                    {
                        volume_++;
                    }
                    else if (!envelope_add_mode_ && volume_ > 0)
                    {
                        volume_--;
                    }
                }
            }

            uint8_t read(uint16_t register_number)
            {
                switch (register_number)
                {
                case 0:
                    return length_ & 0x3F;
                case 1:
                    return (envelope_default_ << 4) | (envelope_add_mode_ << 3) | (length_envelope_ & 0x07);
                case 2:
                    return (shift_clock_frequency_ << 4) | (width_mode_ << 3) | (division_ratio_ & 0x07);
                case 3:
                    return (trigger_ << 7) | (length_enabled_ << 6);
                default:
                    return 0;
                }
            }

            void write(uint8_t value, uint16_t register_number)
            {
                switch (register_number)
                {
                case 0:
                    length_ = value & 0x3F;
                    break;
                case 1:
                    length_envelope_ = value & 0x07;
                    envelope_add_mode_ = (value & 0x08) != 0;
                    envelope_default_ = (value & 0xF0) >> 4;
                    break;
                case 2:
                    division_ratio_ = value & 0x07;
                    width_mode_ = (value & 0x08) != 0;
                    shift_clock_frequency_ = (value & 0xF0) >> 4;
                    break;
                case 3:
                    length_enabled_ = (value & 0x40) != 0;
                    trigger_ = (value & 0x80) != 0;

                    if (trigger_)
                        trigger();
                    break;
                default:
                    break;
                }
            }

            uint8_t getVolume() const
            {
                return output_volume_;
            }

            bool isEnabled() const
            {
                return is_enabled_;
            }

            void disable()
            {
                is_enabled_ = false;
            }

        private:
            void trigger()
            {
                length_counter_ = 64 - length_;
                envelope_timer_ = length_envelope_;
                volume_ = envelope_default_;

                timer_load_ = divisor_table_[division_ratio_] << shift_clock_frequency_;
                timer_ = timer_load_;
            }

            uint8_t length_;
            uint8_t length_envelope_;
            bool envelope_add_mode_;
            uint8_t envelope_default_;
            uint8_t division_ratio_;
            bool width_mode_;
            uint8_t shift_clock_frequency_;
            bool length_enabled_;
            bool trigger_;

            uint8_t volume_;
            uint8_t output_volume_;
            uint8_t length_counter_;
            uint8_t envelope_timer_;
            int timer_load_;
            int timer_;

            std::array<uint8_t, 8> divisor_table_;
            uint16_t lfsr_;

            bool dac_enabled_;
            bool is_enabled_;
        };
    }
}

#endif // GAMEBOY_NOISE_H
