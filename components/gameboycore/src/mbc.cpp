#include "gameboycore/mbc.h"
#include "gameboycore/memorymap.h"

#include <algorithm>
#include <cstring>
#include <cassert>

namespace gb
{
    namespace detail
    {
        MBC::MBC(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enable) :
            xram_enable_(false),
            rom_bank_(0),
            ram_bank_(0),
            num_rom_banks_(0),
            num_cartridge_ram_banks_(0),
            cgb_enabled_(cgb_enable),
            vram_banks_(0),
            num_internal_ram_banks_(0)
        {
            loadMemory(rom, size, rom_size, ram_size);
        }

        void MBC::write(uint8_t value, uint16_t addr)
        {
            // check for write to ROM
            if (addr <= 0x7FFF)
            {
                control(value, addr);
            }
            else
            {
                // get the memory index from the addr
                auto idx = getIndex(addr, rom_bank_, ram_bank_);

                // check if writing to external RAM
                if (addr >= 0xA000 && addr <= 0xBFFF)
                {
                    // write to external ram if it is enabled
                    if (xram_enable_)
                        memory_[idx] = value;
                }
                else
                {
                    memory_[idx] = value;
                }
            }
        }

        uint8_t MBC::read(uint16_t addr) const
        {
            // return FF is read from external ram and it is not enabled
            if (addr >= 0xA000 && addr <= 0xBFFF && !xram_enable_)
            {
                return 0xFF;
            }

            auto idx = getIndex(addr, rom_bank_, ram_bank_);
            return memory_[idx];
        }

        uint8_t MBC::readVram(uint16_t addr, uint8_t bank)
        {
            auto index = (addr) + (16 * KILO_BYTE * (num_rom_banks_ - 1)) + ((8 * KILO_BYTE) * bank);
            return memory_[index];
        }

        uint8_t& MBC::get(uint16_t addr)
        {
            return memory_[getIndex(addr, rom_bank_, ram_bank_)];
        }

        uint8_t* MBC::getptr(uint16_t addr)
        {
            return &memory_[getIndex(addr, rom_bank_, ram_bank_)];
        }

        int MBC::resolveAddress(const uint16_t& addr) const
        {
            return getIndex(addr, rom_bank_, ram_bank_);
        }

        std::vector<uint8_t> MBC::getRange(uint16_t start, uint16_t end) const
        {
            auto start_idx = getIndex(start, rom_bank_, ram_bank_);
            auto end_idx = getIndex(end, rom_bank_, ram_bank_);
            return std::vector<uint8_t>(memory_.begin() + start_idx, memory_.begin() + end_idx);
        }

        void MBC::setMemory(uint16_t start, const std::vector<uint8_t>& mem)
        {
            // TODO: error checks
            std::copy(mem.begin(), mem.end(), memory_.begin() + getIndex(start, rom_bank_, ram_bank_));
        }

        std::vector<uint8_t> MBC::getXram() const
        {
            // index the points around external RAM to capture all bank
            auto start = getIndex(memorymap::EXTERNAL_RAM_START, rom_bank_, 0);
            auto end   = getIndex(memorymap::EXTERNAL_RAM_END, rom_bank_, num_cartridge_ram_banks_ - 1);

            // Copy external RAM range. Add 1 so range [START, END] is inclusive
            return std::vector<uint8_t>(memory_.begin() + start, memory_.begin() + end + 1);
        }

        int MBC::getRomBank() const
        {
            return rom_bank_;
        }

        int MBC::getRamBank() const
        {
            return ram_bank_;
        }

        bool MBC::isXramEnabled() const
        {
            return xram_enable_;
        }

        int MBC::getIndex(uint16_t addr, int rom_bank, int ram_bank) const
        {
            switch (addr & 0xF000)
            {
            case 0x0000:
            case 0x1000:
            case 0x2000:
            case 0x3000:
                return addr;
            case 0x4000:
            case 0x5000:
            case 0x6000:
            case 0x7000:
                return (addr) + (kilo(16) * rom_bank);
            case 0x8000:
            case 0x9000:
                return (addr) + (kilo(16) * (num_rom_banks_-1)) + getVramOffset();
            case 0xA000:
            case 0xB000:
                return (addr) + (kilo(16) * (num_rom_banks_ - 1)) + (kilo(8) * (vram_banks_ - 1)) + (kilo(8) * ram_bank);
            case 0xC000:
                return (addr) + (kilo(16) * (num_rom_banks_ - 1)) + (kilo(8) * (vram_banks_ - 1)) + (kilo(8) * (num_cartridge_ram_banks_-1));
            case 0xD000:
                return (addr)+(kilo(16) * (num_rom_banks_ - 1)) + (kilo(8) * (vram_banks_ - 1)) + (kilo(8) * (num_cartridge_ram_banks_ - 1)) + getInternalRamOffset();
            case 0xE000:
            case 0xF000:
                return (addr)+(kilo(16) * (num_rom_banks_ - 1)) + (kilo(8) * (vram_banks_ - 1)) + (kilo(8) * (num_cartridge_ram_banks_ - 1)) + (kilo(4) * (num_internal_ram_banks_-1));
            }

            return 0;
        }

        int MBC::getIoIndex(uint16_t addr) const
        {
            return	(addr)+
                (kilo(16) * (num_rom_banks_ - 1)) +
                (kilo(8) * (vram_banks_ - 1)) +
                (kilo(8) * (num_cartridge_ram_banks_ - 1)) +
                (kilo(4)* (num_internal_ram_banks_-1));
        }

        int MBC::getVramOffset() const
        {
            return kilo(8) * (memory_[getIoIndex(memorymap::VBK_REGISTER)] & 0x01);
        }

        int MBC::getInternalRamOffset() const
        {
            auto bank_number = memory_[getIoIndex(memorymap::SVBK_REGISTER)] & 0x07;
            if (bank_number < 2) bank_number = 0;

            return kilo(4) * (bank_number);
        }

        void MBC::loadMemory(const uint8_t* rom, std::size_t size, uint8_t rom_size, uint8_t ram_size)
        {
            // lookup tables for number of ROM banks a cartridge has
            static const unsigned int rom_banks1[] = {
                2, 4, 8, 16, 32, 64, 128, 256
            };
            static const unsigned int rom_banks2[] = {
                72, 80, 96
            };

            if (rom_size <= static_cast<uint8_t>(MBC::ROM::MB4))
            {
                // look up the total number of banks this cartridge has
                auto cartridge_rom_banks = rom_banks1[rom_size];
                // the number of switchable ROM banks is 2 less than the total
                // since there are always 2 available in the $0000 - $3FFF range
                num_rom_banks_ = cartridge_rom_banks - 1;
            }
            else
            {
                // the number of switchable ROM banks is 2 less than the total
                // since there are always 2 available in the $0000 - $3FFF range
                num_rom_banks_ = rom_banks2[rom_size] - 1;
            }

            num_cartridge_ram_banks_ = (static_cast<MBC::XRAM>(ram_size) == MBC::XRAM::KB32) ? 4 : 1;

            num_internal_ram_banks_ = (cgb_enabled_) ? 7 : 1;
            vram_banks_ = (cgb_enabled_) ? 2 : 1;

            // memory sizes
            const auto rom_bank0_fixed           = kilo(16);                            // $0000 - $3FFF
            const auto rom_switchable            = kilo(16) * num_rom_banks_;           // $4000 - $7FFF
            const auto vram                      = kilo(8)  * vram_banks_;              // $8000 - $9FFF
            const auto ram_cartridge_switchable  = kilo(8)  * num_cartridge_ram_banks_; // $A000 - $B000
            const auto ram_bank0_fixed           = kilo(4);                             // $C000 - $CFFF
            const auto ram_internal_switchable   = kilo(4)  * num_internal_ram_banks_;  // $D000 - $DFFF
            const auto high_ram                  = kilo(8);                             // $E000 - $FFFF

            const auto memory_size = rom_bank0_fixed + rom_switchable + vram + ram_cartridge_switchable + ram_bank0_fixed + ram_internal_switchable + high_ram;

            memory_.resize(memory_size);

            std::memcpy((char*)&memory_[0], rom, size);
        }

        unsigned int MBC::kilo(unsigned int n) const
        {
            return KILO_BYTE * n;
        }

        std::size_t MBC::getVirtualMemorySize() const
        {
            return memory_.size();
        }

        MBC::~MBC()
        {
        }
    }
}