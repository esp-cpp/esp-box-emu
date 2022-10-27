
/**
    @author Natesh Narain <nnaraindev@gmail.com>
*/


#ifndef GAMEBOYCORE_DISPLAY_H
#define GAMEBOYCORE_DISPLAY_H

#include "gameboycore/mmu.h"
#include "gameboycore/tile.h"
#include "gameboycore/sprite.h"

#include <cstdint>
#include <vector>
#include <array>

namespace gb
{
    namespace detail
    {
        /**
        \brief Class that knows how to read Tile RAM in the Gameboy memory map
        */
        class TileRAM
        {
        public:
            static const unsigned int NUM_TILES = 192;
            static const unsigned int TILE_SIZE = 16;

            using TileRow = std::array<uint8_t, 8>;
            using TileLine = std::array<TileRow, 20>;

        public:
            // TODO: remove this constructor
            TileRAM(MMU& mmu);
            ~TileRAM();

            Tile getSpriteTile(const Sprite& sprite) const;
            std::vector<Tile> getTiles();

            TileRow getRow(int row, uint8_t tilenum, bool umode, uint8_t character_bank = 0);

            template<typename T>
            uint16_t getTileAddress(int32_t base_addr, uint8_t tilenum) const
            {
                return (uint16_t)(base_addr + ((T)tilenum * TILE_SIZE));
            }

        private:
            void setRow(Tile& tile, uint8_t msb, uint8_t lsb, int row) const;

            Tile flipV(const Tile& old) const;
            Tile flipH(const Tile& old) const;

        private:
            uint8_t* tile_ram_;
            MMU& mmu_;
        };
    }
}

#endif // GAMEBOYCORE_DISPLAY_H
