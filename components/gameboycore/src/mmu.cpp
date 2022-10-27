
/**
    \author Natesh Narain <nnaraindev@gmail.com>
*/

#include "gameboycore/mmu.h"
#include "gameboycore/mbc.h"
#include "gameboycore/mbc1.h"
#include "gameboycore/mbc2.h"
#include "gameboycore/mbc3.h"
#include "gameboycore/mbc5.h"
#include "gameboycore/cartinfo.h"
#include "gameboycore/memorymap.h"

#include "bitutil.h"

#include <cstring>
#include <array>
#include <iostream>

namespace gb
{
    /* Private Interface */

    class MMU::Impl
    {
    public:
        Impl(const uint8_t* rom, uint32_t size)
            : oam_updated_{false}
            , cgb_enabled_{false}
        {
            load(rom, size);
        }

        ~Impl()
        {
        }

        void load(const uint8_t* rom, uint32_t size)
        {
            CartInfo header = RomParser::parse(rom);

            cgb_enabled_ = header.cgb_enabled;

            switch (static_cast<detail::MBC::Type>(header.type))
            {
            // Supports MBC1
            // Note: MBC1 handles the ROM only case
            case detail::MBC::Type::ROM_ONLY:
            case detail::MBC::Type::MBC1:
            case detail::MBC::Type::MBC1_RAM:
            case detail::MBC::Type::MBC1_RAM_BAT:
                mbc_.reset(new detail::MBC1(rom, size, header.rom_size, header.ram_size, cgb_enabled_));
                break;

            case detail::MBC::Type::MBC2:
            case detail::MBC::Type::MBC2_BAT:
                mbc_.reset(new detail::MBC2(rom, size, header.rom_size, header.ram_size, cgb_enabled_));
                break;

            case detail::MBC::Type::MBC3:
            case detail::MBC::Type::MBC3_RAM:
            case detail::MBC::Type::MBC3_RAM_BAT:
            case detail::MBC::Type::MBC3_TIME_BAT:
            case detail::MBC::Type::MBC3_TIME_RAM_BAT:
                mbc_.reset(new detail::MBC3(rom, size, header.rom_size, header.ram_size, cgb_enabled_));
                break;

                // TODO: MBC4
            case detail::MBC::Type::MBC5:
            case detail::MBC::Type::MBC5_RAM:
            case detail::MBC::Type::MBC5_RAM_BAT:
            case detail::MBC::Type::MBC5_RUMBLE:
            case detail::MBC::Type::MBC5_RUMBLE_RAM:
            case detail::MBC::Type::MBC5_RUMBLE_RAM_BAT:
                mbc_.reset(new detail::MBC5(rom, size, header.rom_size, header.ram_size, cgb_enabled_));
                break;

            default:
                throw std::runtime_error("Unsupported cartridge type :(");
                break;
            }

            // initialize joypad keys to not pressed
            mbc_->write(0x0F, memorymap::JOYPAD_REGISTER);

            loadResetValues();
        }
    
        uint8_t read(uint16_t addr)
        {
            if (addr >= 0xFF00 && addr <= 0xFF7F && read_handlers_[addr - 0xFF00])
            {
                return read_handlers_[addr - 0xFF00](addr);
            }
            else
            {
                return mbc_->read(addr);
            }
        }

        void write(uint8_t value, uint16_t addr)
        {
            if (addr == memorymap::DMA_REGISTER)
            {
                oamTransfer(value);
            }
            else if (addr == memorymap::JOYPAD_REGISTER)
            {
                mbc_->write(value | 0x0F, addr);
            }
            else if (addr == memorymap::DIVIDER_REGISER)
            {
                mbc_->write(0, addr);
            }
            else
            {
                if (addr >= 0xFF00 && addr <= 0xFF7F && write_handlers_[addr - 0xFF00])
                {
                    write_handlers_[addr - 0xFF00](value, addr);
                }
                else
                {
                    mbc_->write(value, addr);
                }
            }
        }

        void write(uint16_t value, uint16_t addr)
        {
            uint8_t hi = (value & 0xFF00) >> 8;
            uint8_t lo = (value & 0x00FF);

            write(lo, addr + 0);
            write(hi, addr + 1);
        }

        uint8_t readVram(uint16_t addr, uint8_t bank)
        {
            return (cgb_enabled_ || bank == 0) ? mbc_->readVram(addr, bank) : 0;
        }

        void dma(uint16_t dest, uint16_t src, uint16_t n)
        {
            while (n--)
            {
                write(read(src++), dest++);
            }
        }

        void oamTransfer(uint8_t base)
        {
            // increments of $100 bytes
            uint16_t addr = ((base & 0x00FF) << 8);
            // copy to OAM
            std::memcpy(mbc_->getptr(memorymap::OAM_START), mbc_->getptr(addr), memorymap::OAM_END - memorymap::OAM_START);

            // set flag indicating oam transfer has taken place
            oam_updated_ = true;
        }

        std::vector<uint8_t> getBatteryRam() const
        {
            return mbc_->getXram();
        }

        void setBatteryRam(const std::vector<uint8_t>& battery_ram)
        {
            mbc_->setMemory(memorymap::EXTERNAL_RAM_START, battery_ram);
        }

        void setTimeProvider(const TimeProvider provider)
        {
            // The time provider is only valid for MBC3
            if (auto mbc_ptr = static_cast<detail::MBC3*>(mbc_.get()))
            {
                mbc_ptr->setTimeProvider(provider);
            }
        }

        int resolveAddress(const uint16_t& addr) const
        {
            return mbc_->resolveAddress(addr);
        }

        void loadResetValues()
        {
            // load reset values into registers
            mbc_->write(0x00, memorymap::TIMER_COUNTER_REGISTER);
            mbc_->write(0x00, memorymap::TIMER_MODULO_REGISTER);
            mbc_->write(0x00, memorymap::TIMER_CONTROLLER_REGISTER);

            mbc_->write(0x80, memorymap::NR10_REGISTER);
            mbc_->write(0xBF, memorymap::NR11_REGISTER);
            mbc_->write(0xF3, memorymap::NR12_REGISTER);
            mbc_->write(0xBF, memorymap::NR14_REGISTER);
            mbc_->write(0x3F, memorymap::NR21_REGISTER);
            mbc_->write(0x00, memorymap::NR22_REGISTER);
            mbc_->write(0xBF, memorymap::NR24_REGISTER);
            mbc_->write(0x7F, memorymap::NR30_REGISTER);
            mbc_->write(0xFF, memorymap::NR31_REGISTER);
            mbc_->write(0x9F, memorymap::NR32_REGISTER);
            mbc_->write(0xBF, memorymap::NR33_REGISTER);
            mbc_->write(0xFF, memorymap::NR41_REGISTER);
            mbc_->write(0x00, memorymap::NR42_REGISTER);
            mbc_->write(0x00, memorymap::NR43_REGISTER);
            mbc_->write(0xBF, memorymap::NR44_REGISTER);
            mbc_->write(0x77, memorymap::NR50_REGISTER);
            mbc_->write(0xF3, memorymap::NR51_REGISTER);
            mbc_->write(0xF1, memorymap::NR52_REGISTER); // TODO: super gameboy mode

            mbc_->write(0x91, memorymap::LCDC_REGISTER);
            mbc_->write(0x00, memorymap::SCY_REGISTER);
            mbc_->write(0x00, memorymap::SCX_REGISTER);
            mbc_->write(0x00, memorymap::LYC_REGISTER);
            mbc_->write(0xFC, memorymap::BGP_REGISTER);
            mbc_->write(0xFF, memorymap::OBP0_REGISTER);
            mbc_->write(0xFF, memorymap::OBP1_REGISTER);
            mbc_->write(0x00, memorymap::WX_REGISTER);
            mbc_->write(0x00, memorymap::WY_REGISTER);
            mbc_->write(0x00, memorymap::INTERRUPT_ENABLE);
        }

    public:
        detail::MBC::Ptr mbc_;

        std::array<MemoryWriteHandler, 0x80> write_handlers_;
        std::array<MemoryReadHandler, 0x80>  read_handlers_;

        bool oam_updated_;
        bool cgb_enabled_;
    };


    /* Public Interface */

    MMU::MMU(const uint8_t* rom, uint32_t size) :
        impl_(new Impl(rom, size))
    {
    }

    MMU::~MMU()
    {
        delete impl_;
    }

    uint8_t MMU::read(uint16_t addr) const
    {
        return impl_->mbc_->read(addr);
    }

    uint8_t MMU::read(uint16_t addr)
    {
        return impl_->read(addr);
    }

    void MMU::write(uint8_t value, uint16_t addr)
    {
        impl_->write(value, addr);
    }

    void MMU::write(uint16_t value, uint16_t addr)
    {
        impl_->write(value, addr);
    }

    uint8_t MMU::readVram(uint16_t addr, uint8_t bank)
    {
        return impl_->readVram(addr, bank);
    }

    void MMU::dma(uint16_t dest, uint16_t src, uint16_t n)
    {
        impl_->dma(dest, src, n);
    }

    void MMU::addWriteHandler(uint16_t addr, MemoryWriteHandler handler)
    {
        impl_->write_handlers_[addr - 0xFF00] = handler;
    }

    void MMU::addReadHandler(uint16_t addr, MemoryReadHandler handler)
    {
        impl_->read_handlers_[addr - 0xFF00] = handler;
    }

    std::vector<uint8_t> MMU::getBatteryRam() const
    {
        return impl_->getBatteryRam();
    }

    void MMU::setBatteryRam(const std::vector<uint8_t>& battery_ram)
    {
        impl_->setBatteryRam(battery_ram);
    }

	void MMU::setTimeProvider(const TimeProvider provider)
	{
		impl_->setTimeProvider(provider);
	}

    bool MMU::getOamTransferStatus() const
    {
        bool ret = impl_->oam_updated_;
        impl_->oam_updated_ = false;

        return ret;
    }

    bool MMU::cgbEnabled() const
    {
        return impl_->cgb_enabled_;
    }

    uint8_t& MMU::get(uint16_t addr)
    {
        return impl_->mbc_->get(addr);
    }

    uint8_t* MMU::getptr(uint16_t addr)
    {
        return impl_->mbc_->getptr(addr);
    }

    int MMU::resolveAddress(const uint16_t& addr) const
    {
        return impl_->resolveAddress(addr);
    }

    std::size_t MMU::getVirtualMemorySize() const
    {
        return impl_->mbc_->getVirtualMemorySize();
    }
}
