/**
 * \file pixel.h
 * \author Natesh Narain <nnaraindev@gmail.com>
 * \date Oct 23 2016
*/

#ifndef GAMEBOYCORE_PIXEL_H
#define GAMEBOYCORE_PIXEL_H

#include <cstdint>

namespace gb
{
    //! Pixel type
    struct Pixel
    {
        Pixel() :
            r(0), g(0), b(0)
        {
        }

        Pixel(uint8_t r, uint8_t g, uint8_t b) :
            r(r), g(g), b(b)
        {
        }

        explicit Pixel(uint8_t v) : r(v), g(v), b(v)
        {
        }

        uint8_t r; ///< Red
        uint8_t g; ///< Green
        uint8_t b; ///< Blue

        bool operator==(const Pixel& rhs)
        {
            return this->r == rhs.r && this->g == rhs.g && this->b == rhs.b;
        }
    };
}

#endif // GAMEBOYCORE_PIXEL_H
