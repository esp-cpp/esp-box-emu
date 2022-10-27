
/**
    \file mbc.h
    \brief Interface memory bank controllers
    \author Natesh Narain <nnaraindev@gmail.com>
    \date   Oct 11 2016

    \defgroup MBC Memory Bank Controllers
*/

#ifndef GAMEBOYCORE_MBC_H
#define GAMEBOYCORE_MBC_H

#include <cstdint>
#include <vector>
#include <memory>

namespace gb
{
    namespace detail
    {
        enum {
            KILO_BYTE = 1024,
            BANK_SIZE = (16 * KILO_BYTE)
        };

        /**
            \class MBC
            \brief Memory Bank Controller Interface
            \ingroup MBC
        */
        class MBC
        {
        public:
            enum class Type {
                ROM_ONLY            = 0x00,
                MBC1                = 0x01,
                MBC1_RAM            = 0x02,
                MBC1_RAM_BAT        = 0x03,
                MBC2                = 0x05,
                MBC2_BAT            = 0x06,
                ROM_RAM             = 0x08,
                ROM_RAM_BAT         = 0x09,
                MMM01               = 0x0B,
                MMM01_RAM           = 0x0C,
                MMM01_RAM_BAT       = 0x0D,
                MBC3_TIME_BAT       = 0x0F,
                MBC3_TIME_RAM_BAT   = 0x10,
                MBC3                = 0x11,
                MBC3_RAM            = 0x12,
                MBC3_RAM_BAT        = 0x13,
                MBC4                = 0x15,
                MBC4_RAM            = 0x16,
                MBC4_RAM_BAT        = 0x17,
                MBC5                = 0x19,
                MBC5_RAM            = 0x1A,
                MBC5_RAM_BAT        = 0x1B,
                MBC5_RUMBLE         = 0x1C,
                MBC5_RUMBLE_RAM     = 0x1D,
                MBC5_RUMBLE_RAM_BAT = 0x1E
            };

            //! ROM types specified in cartridge header
            enum class ROM
            {
                KB32  = 0x00,	///< 32  kB
                KB64  = 0x01,	///< 64  kB
                KB128 = 0x02,	///< 128 kB
                KB256 = 0x03,	///< 256 kB
                KB512 = 0x04,	///< 512 kB
                MB1   = 0x05,	///< 1   MB
                MB2   = 0x06,	///< 2   MB
                MB4   = 0x07,	///< 4   MB
                MB1_1 = 0x52,	///< 1.1 MB
                MB1_2 = 0x53,	///< 1.2 MB
                MB1_5 = 0x54	///< 1.5 MB
            };

            //! External RAM types specified in the cartridge header
            enum class XRAM
            {
                NONE = 0x00, ///< No External RAM
                KB2  = 0x01, ///< 2 kB of External RAM
                KB8  = 0x02, ///< 8 kB of External RAM
                KB32 = 0x03  ///< (4 x 8 kB) of External RAM
            };

        public:
            using Ptr = std::unique_ptr<MBC>;

        public:
            MBC(const uint8_t* rom, uint32_t size, uint8_t rom_size, uint8_t ram_size, bool cgb_enable = false);
            virtual ~MBC();

            virtual void write(uint8_t value, uint16_t addr);
            virtual uint8_t read(uint16_t addr) const;

            uint8_t readVram(uint16_t addr, uint8_t bank);

            uint8_t& get(uint16_t addr);
            uint8_t* getptr(uint16_t addr);

            /**
                Get the virtual memory location from the logical address
            */
            int resolveAddress(const uint16_t& addr) const;

            std::vector<uint8_t> getRange(uint16_t start, uint16_t end) const;
            void setMemory(uint16_t start, const std::vector<uint8_t>& mem);

            std::vector<uint8_t> getXram() const;

            int getRomBank() const;
            int getRamBank() const;
            bool isXramEnabled() const;

            std::size_t getVirtualMemorySize() const;

        protected:
            /**
                Called when a write to ROM occurs
            */
            virtual void control(uint8_t value, uint16_t addr) = 0;

            //! virtual memory
            // std::vector<uint8_t> memory_;
            uint8_t *memory_;
            size_t memory_size_;
            //! Flag inidicating if external ram is enabled
            bool xram_enable_;
            //! ROM bank number
            int rom_bank_;
            //! RAM bank number
            int ram_bank_;

        private:
            /**
                \return index of address into virtual memory
            */
            int getIndex(uint16_t addr, int rom_bank, int ram_bank) const;

            /**
            */
            int getIoIndex(uint16_t addr) const;

            /**
                Get the VRAM offset given the current state of the VBK register
            */
            int getVramOffset() const;

            /**
                Get the internal ram bank offset given the current state of the SVBK register
            */
            int getInternalRamOffset() const;

            /**
            */
            unsigned int kilo(unsigned int n) const;

            /**
                Load memory
            */
            void loadMemory(const uint8_t* rom, std::size_t size, uint8_t rom_size, uint8_t ram_size);

            //! number of switchable rom banks
            int num_rom_banks_;
            //! number of cartridge ram banks
            int num_cartridge_ram_banks_;
            //! CGB enabled
            bool cgb_enabled_;
            //! CGB mode has 2 vram banks for character and map data
            int vram_banks_;
            //! number internal ram banks
            int num_internal_ram_banks_;
        };
    }
}

#endif // GAMEBOYCORE_MBC_H
