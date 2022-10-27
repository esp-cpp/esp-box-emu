/**
    @file cartinfo.h
    @author Natesh Narain <nnaraindev@gmail.com>
*/

#ifndef GAMEBOYCORE_CARTINFO_H
#define GAMEBOYCORE_CARTINFO_H

#include "gameboycore/memorymap.h"
#include <cstdint>

namespace gb
{
    /**
        Structure containing cartridge information, contained in header
    */
    struct CartInfo
    {
        uint8_t type;
        uint8_t rom_size;
        uint8_t ram_size;
        char game_title[(memorymap::GAME_TITLE_END - memorymap::GAME_TITLE_START) + 1];
        bool cgb_enabled;
    };

    /**
        \brief Used to parse rom image for information contained in header
    */
    class RomParser
    {
    public:
        RomParser();

        static CartInfo parse(const uint8_t* image);
    };
}

#endif
