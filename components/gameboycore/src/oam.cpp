#include "gameboycore/oam.h"
#include "bitutil.h"

namespace gb
{
    OAM::OAM(MMU& mmu) : 
        mmu_(mmu)
    {
    }

    Sprite OAM::getSprite(std::size_t idx) const
    {
        // get location of sprite in memory
        auto sprite_base = uint16_t(memorymap::OAM_START + (idx * 4));

        auto ptr = mmu_.getptr(sprite_base);

        // read OAM attributes from OAM table
        Sprite sprite;
        sprite.y    = ptr[0];
        sprite.x    = ptr[1];
        sprite.tile = ptr[2];
        sprite.attr = ptr[3];

        return sprite;
    }

    std::array<Sprite, 40> OAM::getSprites() const
    {
        // check if sprites are 8x16 or 8x8
        auto lcdc = mmu_.read(memorymap::LCDC_REGISTER);
        const bool mode_8x16 = isSet(lcdc, memorymap::LCDC::OBJ_8x16) != 0;

        std::array<Sprite, 40> sprites;

        for (auto i = 0u; i < sprites.size(); ++i)
        {
            auto& sprite = sprites[i];
            sprite = getSprite(i);
            
            if (mode_8x16)
            {
                sprite.height = 16;
            }
            else
            {
                sprite.height = 8;
            }
        }

        return sprites;
    }

    OAM::~OAM()
    {
    }
}