
#include "gameboycore/mbc3.h"

namespace gb
{
    namespace detail
    {
        MBC3::MBC3(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enable)
			: MBC(rom, size, rom_size, ram_size, cgb_enable)
			, latch_ctl_{0}
        {
        }

        uint8_t MBC3::read(uint16_t addr) const
        {
            if (addr >= 0xA000 && addr <= 0xBFFF && rtc_.isEnabled())
            {
                return rtc_.get();
            }
            else
            {
                return MBC::read(addr);
            }
        }

        void MBC3::control(uint8_t value, uint16_t addr)
        {
            if (addr <= 0x1FFF)
            {
                xram_enable_ = ((value & 0x0F) == 0x0A);
            }
            else if (addr >= 0x2000 && addr <= 0x3FFF)
            {
                auto bank_select = (value == 0) ? 1 : value;
                rom_bank_ = bank_select - 1;
            }
            else if (addr >= 0x4000 && addr <= 0x5FFF)
            {
                // is a RAM bank number
                if (value <= 0x03)
                {
                    ram_bank_ = value & 0x0F;
                    rtc_.setEnable(false);
                }
                else if (value >= 0x08 && value <= 0x0C)
                {
                    rtc_.setEnable(true);
                    rtc_.select(value);
                }
            }
            else if(addr >= 0x6000 && addr <= 0x7FFF)
            {
				if (value == 0x00)
				{
					latch_ctl_++;
				}
				else if (latch_ctl_ == 1 && value == 0x01)
				{
					rtc_.latch();
					latch_ctl_ = 0;
				}
				else
				{
					latch_ctl_ = 0;
				}
            }
        }

		void MBC3::setTimeProvider(TimeProvider provider)
		{
			rtc_.setTimeProvider(provider);
		}

        MBC3::~MBC3()
        {
        }
    }
}
