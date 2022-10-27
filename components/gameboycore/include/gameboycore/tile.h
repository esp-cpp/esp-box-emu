#ifndef GAMEBOYCORE_TILE_H
#define GAMEBOYCORE_TILE_H

#include <cstdint>

namespace gb
{
    struct Tile
    {
        uint8_t color[64];
    };
}

#endif // GAMEBOYCORE_TILE_H
