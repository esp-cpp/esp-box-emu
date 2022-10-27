
#include "gameboycore/tileram.h"
#include "gameboycore/memorymap.h"

#include <stdexcept>

namespace gb
{
    namespace detail
    {
        TileRAM::TileRAM(MMU& mmu) :
            tile_ram_(mmu.getptr(0x8000)),
            mmu_(mmu)
        {
        }

        TileRAM::TileRow TileRAM::getRow(int row, uint8_t tilenum, bool umode, uint8_t character_bank)
        {
            TileRow tile_row;

            // get the tile address depending on whether current using unsigned mode or signed mode
            uint16_t addr = (umode)
                ? getTileAddress<uint8_t>(0x8000, tilenum)
                : getTileAddress<int8_t>(0x9000, tilenum);

            auto row_offset = uint16_t(row * 2);
            auto lsb = mmu_.readVram(addr + row_offset, character_bank);
            auto msb = mmu_.readVram(addr + row_offset + 1, character_bank);

            auto idx = 0;
            for (auto bit = (int)(tile_row.size() - 1); bit >= 0; --bit)
            {
                uint8_t mask = (1 << bit);
                uint8_t color = (((msb & mask) >> bit) << 1) | ((lsb & mask) >> bit);

                tile_row[idx++] = color;
            }

            return tile_row;
        }

        Tile TileRAM::getSpriteTile(const Sprite& sprite) const
        {
            auto tile_ptr = mmu_.getptr(getTileAddress<uint8_t>(0x8000, sprite.tile));

            Tile tile;
            auto row = 0;

            for (auto i = 0u; i < TILE_SIZE; i += 2)
            {
                uint8_t lsb = tile_ptr[i + 0];
                uint8_t msb = tile_ptr[i + 1];

                setRow(tile, msb, lsb, row);
                row++;
            }

            // apply sprite attributes
            if (sprite.isVerticallyFlipped())
                tile = flipV(tile);
            if (sprite.isHorizontallyFlipped())
                tile = flipH(tile);

            return tile;
        }

        Tile TileRAM::flipV(const Tile& old) const
        {
            static const auto NUM_ROWS = 8;
            static const auto PIXELS_PER_ROW = 8;

            Tile tile;

            for (auto row = 0; row < NUM_ROWS; ++row)
            {
                auto target_row = NUM_ROWS - row - 1;

                auto old_idx = (row * PIXELS_PER_ROW);
                auto target_idx = (target_row * PIXELS_PER_ROW);

                tile.color[target_idx + 0] = old.color[old_idx + 0];
                tile.color[target_idx + 1] = old.color[old_idx + 1];
                tile.color[target_idx + 2] = old.color[old_idx + 2];
                tile.color[target_idx + 3] = old.color[old_idx + 3];
                tile.color[target_idx + 4] = old.color[old_idx + 4];
                tile.color[target_idx + 5] = old.color[old_idx + 5];
                tile.color[target_idx + 6] = old.color[old_idx + 6];
                tile.color[target_idx + 7] = old.color[old_idx + 7];
            }

            return tile;
        }

        Tile TileRAM::flipH(const Tile& old) const
        {
            static const auto NUM_COLS = 8;
            static const auto NUM_ROWS = 8;
            static const auto PIXELS_PER_ROW = 8;

            Tile tile;

            for (auto col = 0; col < NUM_COLS; ++col)
            {
                auto old_idx = col;
                auto target_idx = NUM_COLS - col;

                for (auto row = 0; row < NUM_ROWS; ++row)
                {
                    auto row_offset = (row * PIXELS_PER_ROW) - 1;
                    tile.color[target_idx + row_offset] = old.color[old_idx + row_offset];
                }
            }

            return tile;
        }

        std::vector<Tile> TileRAM::getTiles()
        {
            std::vector<Tile> tiles;

            for (auto i = 0u; i < NUM_TILES; i++)
            {
                unsigned int offset = i * TILE_SIZE;
                uint8_t* current_tile_ptr = tile_ram_ + offset;

                Tile tile;
                int row = 0;

                for (auto j = 0u; j < TILE_SIZE; j += 2)
                {
                    uint8_t lsb = current_tile_ptr[j + 0];
                    uint8_t msb = current_tile_ptr[j + 1];

                    setRow(tile, msb, lsb, row);
                    row++;
                }

                tiles.push_back(tile);
            }

            return tiles;
        }

        void TileRAM::setRow(Tile& tile, uint8_t msb, uint8_t lsb, int row) const
        {
            int offset = row * 8;

            int pixel = 0;
            for (int i = 7; i >= 0; i--)
            {
                uint8_t mask = (1 << i);
                uint8_t color = (((msb & mask) >> i) << 1) | ((lsb & mask) >> i);

                tile.color[offset + pixel] = color;
                pixel++;
            }
        }

        TileRAM::~TileRAM()
        {
        }
    }
}
