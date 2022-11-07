#pragma once
#include "common.hpp"
#include "sprite.hpp"
#include "tiledata.hpp"
#include <cstdint>
#include <vector>

#include "spi_lcd.h"

namespace gbc
{
enum dmg_variant_t
{
    LIGHTER_GREEN = 0,
    DARKER_GREEN,
    GRAYSCALE
};
class GPU
{
public:
    static const int SCREEN_W = 160;
    static const int SCREEN_H = 144;
    static const int NUM_PALETTES = 64;
    // this palette idx is used when the screen is off
	static const int WHITE_IDX = 32;
#ifdef GAMEBRO_INDEXED_FRAME
	using PixelType = uint8_t;
#else
	using PixelType = uint16_t;
#endif

    GPU(Machine&) noexcept;
    void reset() noexcept;
    void simulate();
    // the vector is resized to exactly fit the screen
    const auto& pixels() const noexcept { return m_pixels; }
    // trap on palette changes
    using palchange_func_t = std::function<void(uint8_t idx, uint16_t clr)>;
    void on_palchange(palchange_func_t func) { m_on_palchange = func; }
    // get default GB palette
    static std::array<uint32_t, 4> dmg_colors(dmg_variant_t = GRAYSCALE);
    // set GB palette used in RGBA mode
    void set_dmg_variant(dmg_variant_t);
    // get the 32-bit RGB colors (with alpha=0)
    uint32_t expand_cgb_color(uint8_t idx) const noexcept;
    uint32_t expand_dmg_color(uint8_t idx) const noexcept;
	static uint32_t color15_to_rgba32(uint16_t color15);
    // enable / disable scanline rendering
    void scanline_rendering(bool en) noexcept { this->m_render = en; }
    // render whole frame now (NOTE: changes are often made mid-frame!)
    void render_frame();

    bool is_vblank() const noexcept;
    bool is_hblank() const noexcept;
    int current_scanline() const noexcept { return m_state.current_scanline; }
    uint64_t frame_count() const noexcept { return m_state.frame_count; }

    void set_mode(uint8_t mode);
    uint8_t get_mode() const noexcept;

    uint16_t video_offset() const noexcept { return m_state.video_offset; }
    void set_video_bank(uint8_t bank);
    void lcd_power_changed(bool state);

    bool lcd_enabled() const noexcept;
    bool window_enabled() const noexcept;
    std::pair<int, int> window_size();
    bool window_visible();
    int window_x();
    int window_y();

    // CGB palette registers
    uint8_t& getpal(uint16_t index) noexcept { return m_state.cgb_palette[index]; }
    void setpal(uint16_t index, uint8_t value);
    uint8_t getpal(uint16_t index) const { return m_state.cgb_palette.at(index); }

    // serialization
    int restore_state(const std::vector<uint8_t>&, int);
    void serialize_state(std::vector<uint8_t>&) const;

    Machine& machine() noexcept { return m_memory.machine(); }
    Memory& memory() noexcept { return m_memory; }
    IO& io() noexcept { return m_io; }
    const Memory& memory() const noexcept { return m_memory; }
    std::vector<uint16_t> dump_background();
    std::vector<uint16_t> dump_window();
    std::vector<uint16_t> dump_tiles(int bank);
    // OAM sprite inspection
    const Sprite* sprites_begin() const noexcept;
    const Sprite* sprites_end() const noexcept;

private:
    uint64_t scanline_cycles() const noexcept;
    uint64_t oam_cycles() const noexcept;
    uint64_t vram_cycles() const noexcept;
    uint64_t hblank_cycles() const noexcept;
    void render_scanline(int y);
    void do_ly_comparison();
    TileData create_tiledata(uint16_t tiles, uint16_t patt);
    tileconf_t tile_config();
    sprite_config_t sprite_config();
    std::vector<const Sprite*> find_sprites(const sprite_config_t&) const;
    uint16_t colorize_tile(const tileconf_t&, uint8_t attr, uint8_t idx);
    uint16_t colorize_sprite(const Sprite*, sprite_config_t&, uint8_t);
    // addresses
    uint16_t bg_tiles() const noexcept;
    uint16_t window_tiles() const noexcept;
    uint16_t tile_data() const noexcept;

    Memory& m_memory;
    IO& m_io;
    uint8_t& m_reg_lcdc;
    uint8_t& m_reg_stat;
    uint8_t& m_reg_ly;
	std::vector<PixelType> m_pixels;
    palchange_func_t m_on_palchange = nullptr;
    dmg_variant_t m_variant = LIGHTER_GREEN;
    bool m_render = true;

    struct state_t
    {
        uint64_t period = 0;
        uint64_t frame_count = 0;
        int current_scanline = 0;
        uint16_t video_offset = 0x0;
        bool white_frame = false;
        // 0-63: tiles 64-127: sprites
        std::array<uint8_t, 128> cgb_palette;
    } m_state;
};

inline std::array<uint32_t, 4> GPU::dmg_colors(dmg_variant_t variant)
{
#define mRGB(r, g, b) (make_color(r,g,b))
    switch (variant)
    {
    case LIGHTER_GREEN:
        return std::array<uint32_t, 4>{
            mRGB(224, 248, 208), // least green
            mRGB(136, 192, 112), // less green
            mRGB(52, 104, 86),   // very green
            mRGB(8, 24, 32)      // dark green
        };
    case DARKER_GREEN:
        return std::array<uint32_t, 4>{
            mRGB(175, 203, 70),  // least green
            mRGB(121, 170, 109), // less green
            mRGB(34, 111, 95),   // very green
            mRGB(8, 41, 85)      // dark green
        };
    case GRAYSCALE:
    default:
        return std::array<uint32_t, 4>{
            mRGB(232, 232, 232), // least gray
            mRGB(160, 160, 160), // less gray
            mRGB(88, 88, 88),    // very gray
            mRGB(16, 16, 16)     // dark gray
        };
    }
#undef mRGB
}

// convert palette to grayscale colors
inline uint32_t GPU::expand_dmg_color(const uint8_t index) const noexcept
{
    return dmg_colors(m_variant).at(index);
}
// convert 15-bit color to 32-bit RGBA
inline uint32_t GPU::expand_cgb_color(const uint8_t index) const noexcept
{
    const uint16_t color15 = getpal(index * 2) | (getpal(index * 2 + 1) << 8);
    const uint16_t r = ((color15 >> 0) & 0x1f) << 3;
    const uint16_t g = ((color15 >> 5) & 0x1f) << 3;
    const uint16_t b = ((color15 >> 10) & 0x1f) << 3;
    return r | (g << 8) | (b << 16);
}
inline uint32_t GPU::color15_to_rgba32(const uint16_t color15)
{
	const uint16_t r = ((color15 >> 0) & 0x1f) << 3;
    const uint16_t g = ((color15 >> 5) & 0x1f) << 3;
    const uint16_t b = ((color15 >> 10) & 0x1f) << 3;
    return (r << 0u) | (g << 8u) | (b << 16u) | (255ul << 24u);
}
} // namespace gbc
