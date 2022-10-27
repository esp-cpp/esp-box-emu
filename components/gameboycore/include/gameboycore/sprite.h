
/**
    @author Natesh Narain <nnaraindev@gmail.com>
*/

#ifndef GAMEBOYCORE_SPRITE_H
#define GAMEBOYCORE_SPRITE_H

#include <cstdint>

namespace gb
{
    /**
        Sprite object that are stored in OAM
    */
    class Sprite
    {
    public:
        uint8_t y;    ///< y pixel coordinate
        uint8_t x;    ///< x pixel coordinate
        uint8_t tile; ///< tile number
        uint8_t attr; ///< attribute data

        uint8_t height; ///< sprite height in pixels

        Sprite()
            : y{0}
            , x{0}
            , tile{0}
            , attr{0}
            , height{0}
        {
        }

        bool isHorizontallyFlipped() const
        {
            return (attr & (1 << 5)) != 0;
        }

        bool isVerticallyFlipped() const
        {
            return (attr & (1 << 6)) != 0;
        }

        bool hasPriority() const
        {
            return (attr & (1 << 7)) == 0;
        }

        uint8_t paletteOBP0() const
        {
            return !!(attr & (1 << 4));
        }

        uint8_t getCgbPalette() const
        {
            return (attr & 0x07);
        }

        uint8_t getCharacterBank() const
        {
            return !!(attr & (1 << 3));
        }

        bool operator==(const Sprite& rhs)
        {
            return
                this->y == rhs.y &&
                this->x == rhs.x &&
                this->tile == rhs.tile &&
                this->attr == rhs.attr &&
                this->height == rhs.height;
        }
    };
}

#endif // GAMEBOYCORE_SPRITE_H
