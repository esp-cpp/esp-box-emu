
#include "gameboycore/cartinfo.h"
#include "gameboycore/mbc.h"
#include <cstring>

namespace gb
{
    RomParser::RomParser()
    {
    }

    CartInfo RomParser::parse(const uint8_t* image)
    {
        CartInfo info;
        info.type = image[memorymap::CART_TYPE];
        info.rom_size = image[memorymap::CART_ROM_SIZE];
        info.ram_size = image[memorymap::CART_RAM_SIZE];
        std::memcpy(
            info.game_title,
            &image[memorymap::GAME_TITLE_START],
            memorymap::GAME_TITLE_END - memorymap::GAME_TITLE_START
        );
        info.game_title[(memorymap::GAME_TITLE_END - memorymap::GAME_TITLE_START)] = '\0';

        auto cgb_flag = image[memorymap::COLOR_COMPATABILITY];
        info.cgb_enabled = (cgb_flag == 0x80) || (cgb_flag == 0xC0);

        return info;
    }
}
