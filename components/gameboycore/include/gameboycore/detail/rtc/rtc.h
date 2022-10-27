/**
    \file rtc.h
    \brief Real Time Clock Emulation
    \author Natesh Narain
*/

#ifndef GAMEBOYCORE_RTC_H
#define GAMEBOYCORE_RTC_H

#include <gameboycore/time.h>

#include <array>
#include <ctime>
#include <functional>

namespace gb
{
    namespace detail
    {
        /**
            \class RTC
            \brief Real Time Clock
            \ingroup MBC
        */
        class RTC
        {
        private:
            static constexpr uint8_t REGISTER_BASE = 0x08;

            enum Registers
            {
                SECONDS_REGISTER = 0,
                MINUTES_REGISTER,
                HOURS_REGISTER,
                DAY_LSB_REGISTER,
                DAY_MSB_REGISTER
            };

        public:
            RTC()
                : enabled_{false}
                , selected_{0}
                , time_data_{0, 0, 0}
            {
                time_provider_ = RTC::getTime;
            }

            ~RTC()
            {
            }

            uint8_t get() const
            {
                return time_data_[selected_];
            }

            void setEnable(bool enable)
            {
                enabled_ = enable;
            }

            void latch()
            {
                const auto time = time_provider_();
                time_data_[0] = time.seconds;
                time_data_[1] = time.minutes;
                time_data_[2] = time.hours;
                time_data_[3] = time.days & 0xFF;
                time_data_[4] = (time.days & 0x100) >> 8;
            }

            void select(uint8_t reg)
            {
                selected_ = reg - REGISTER_BASE;
            }
            
            bool isEnabled() const
            {
                return enabled_;
            }

            void setTimeProvider(TimeProvider fn)
            {
                time_provider_ = fn;
            }

            static const Time getTime()
            {
                const auto time = std::time(0);
                const auto now = std::localtime(&time);

                const auto seconds = static_cast<uint8_t>(now->tm_sec);
                const auto minutes = static_cast<uint8_t>(now->tm_min);
                const auto hours = static_cast<uint8_t>(now->tm_hour);
                const auto days = static_cast<uint8_t>(now->tm_yday);

                return Time{ seconds, minutes, hours, days };
            }

        private:
            bool enabled_;
            uint8_t selected_;
            std::array<uint8_t, 5> time_data_;
            TimeProvider time_provider_;
        };
    }
}

#endif // GAMEBOYCORE_RTC_H
