/**
 * \file palette.h
 * \author Natesh Narain
*/

#ifndef GAMEBOYCORE_PALETTE_H
#define GAMEBOYCORE_PALETTE_H

#include "gameboycore/pixel.h"

#include <array>
#include <cstdint>

namespace gb
{
    class Palette
    {
    public:
        Palette()
        {
            reset();
        }

        std::array<Pixel, 4> get(uint8_t reg)
        {
            std::array<Pixel, 4> palette;

            auto color3 = (reg & 0xC0) >> 6;
            auto color2 = (reg & 0x30) >> 4;
            auto color1 = (reg & 0x0C) >> 2;
            auto color0 = (reg & 0x03);

            palette[3] = colors_[color3];
            palette[2] = colors_[color2];
            palette[1] = colors_[color1];
            palette[0] = colors_[color0];

            return palette;
        }

        void reset()
        {
            colors_[0] = Pixel(255);
            colors_[1] = Pixel(192);
            colors_[2] = Pixel(96);
            colors_[3] = Pixel(0);
        }

        void set(uint8_t r, uint8_t g, uint8_t b, int idx)
        {
            colors_[idx] = Pixel(r, g, b);
        }

    private:
        std::array<Pixel, 4> colors_;
    };
}

#endif // GAMEBOYCORE_PALETTE_H
