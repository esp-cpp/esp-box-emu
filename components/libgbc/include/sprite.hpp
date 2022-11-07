#pragma once
#include "memory.hpp"

namespace gbc
{
struct sprite_config_t
{
    const uint8_t* patterns;
    uint8_t palette[2];
    int scan_x;
    int scan_y;
    int height;
    bool is_cgb;

    void set_height(bool mode8x16) { height = mode8x16 ? 16 : 8; }
};

class Sprite
{
public:
    static const int SPRITE_W = 8;

    Sprite() = delete;

    bool hidden() const noexcept { return ypos == 0 || ypos >= 160 || xpos == 0 || xpos >= 168; }
    uint8_t pattern_idx() const noexcept { return pattern; }
    bool behind() const noexcept { return attr & 0x80; }
    bool flipx() const noexcept { return attr & 0x20; }
    bool flipy() const noexcept { return attr & 0x40; }
    int pal() const noexcept { return (attr & 0x10) >> 4; }
    int cgb_bank() const noexcept { return (attr & 0x8) >> 3; }
    int cgb_pal() const noexcept { return attr & 0x7; }

    uint8_t pixel(const sprite_config_t&) const;

    int start_x() const noexcept { return xpos - 8; }
    int start_y() const noexcept { return ypos - 16; }

    bool is_within_scanline(const sprite_config_t& config) const noexcept
    {
        return config.scan_y >= start_y() && config.scan_y < start_y() + config.height;
    }

private:
    uint8_t ypos;
    uint8_t xpos;
    uint8_t pattern;
    uint8_t attr;
};

inline uint8_t Sprite::pixel(const sprite_config_t& config) const
{
    int tx = config.scan_x - start_x();
    int ty = config.scan_y - start_y();
    if (tx < 0 || tx >= SPRITE_W) return 0;
    if (this->flipx()) tx = SPRITE_W - 1 - tx;
    if (this->flipy()) ty = config.height - 1 - ty;

    int offset = this->pattern * 16 + ty * 2;
    if (config.is_cgb) offset += cgb_bank() * 0x2000;
    uint8_t c0 = config.patterns[offset];
    uint8_t c1 = config.patterns[offset + 1];
    // return combined 4-bits, right to left
    const int bit = 7 - tx;
    const int v0 = (c0 >> bit) & 0x1;
    const int v1 = (c1 >> bit) & 0x1;
    return v0 | (v1 << 1);
}
} // namespace gbc
