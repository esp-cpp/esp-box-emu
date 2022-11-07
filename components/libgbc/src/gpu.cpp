#include "gpu.hpp"

#include "machine.hpp"
#include "sprite.hpp"
#include "tiledata.hpp"
#include <cassert>
#include <unistd.h>

namespace gbc
{
const int GPU::WHITE_IDX;
GPU::GPU(Machine& mach) noexcept
    : m_memory(mach.memory)
    , m_io(mach.io)
    , m_reg_lcdc{io().reg(IO::REG_LCDC)}
    , m_reg_stat{io().reg(IO::REG_STAT)}
    , m_reg_ly{io().reg(IO::REG_LY)}
{
    this->reset();
}

void GPU::reset() noexcept
{
    m_pixels.resize(SCREEN_W * SCREEN_H);
    this->m_state.video_offset = 0;
    // set_mode((m_reg_ly >= 144) ? 1 : 2);
}
uint64_t GPU::scanline_cycles() const noexcept
{
    return oam_cycles() + vram_cycles() + hblank_cycles();
}
uint64_t GPU::oam_cycles() const noexcept
{
    return 83 * memory().speed_factor();
    // return memory().speed_factor() * 80;
}
uint64_t GPU::vram_cycles() const noexcept
{
    return 175 * memory().speed_factor();
    // return memory().speed_factor() * 172;
}
uint64_t GPU::hblank_cycles() const noexcept
{
    return 207 * memory().speed_factor();
    // return memory().speed_factor() * 204;
}
void GPU::simulate()
{
    // nothing to do with LCD being off
    if (!this->lcd_enabled()) { return; }

    auto& vblank = io().vblank;
    auto& lcd_stat = io().lcd_stat;

    this->m_state.period += 4;
    const uint64_t period = this->m_state.period;
    // assert(period == 4);
    const bool new_scanline = period >= scanline_cycles();

    // scanline logic when screen on
    if (UNLIKELY(new_scanline))
    {
        this->m_state.period = 0; // start over each scanline
        // scanline LY increment logic
        static const int MAX_LINES = 154;
        m_state.current_scanline = (m_state.current_scanline + 1) % MAX_LINES;
        m_reg_ly = m_state.current_scanline;

        if (UNLIKELY(m_reg_ly == 144))
        {
            if (this->m_state.white_frame)
            {
                this->m_state.white_frame = false;
                // create white palette value at color 32
                if (this->m_on_palchange) { this->m_on_palchange(WHITE_IDX, 0xFFFF); }
                if (LIKELY(this->m_render))
                {
                    // clear pixelbuffer with white
                    std::fill_n(m_pixels.begin(), m_pixels.size(), WHITE_IDX);
                }
            }
            // enable MODE 1: V-blank
            set_mode(1);
            // MODE 1: vblank interrupt
            io().trigger(vblank);
            // modify stat
            this->set_mode(1);
            // if STAT vblank interrupt is enabled
            if (m_reg_stat & 0x10) io().trigger(lcd_stat);
        }
        else if (m_reg_ly == 1 && get_mode() == 1)
        {
            assert(this->is_vblank());
            // start over in regular mode
            this->m_state.current_scanline = 0;
            this->m_reg_ly = 0;
            set_mode(0);
            // new frame
            m_state.frame_count++;
        }
        // LY == LYC comparison on each line
        this->do_ly_comparison();
    }
    // STAT mode & scanline period modulation
    if (!this->is_vblank())
    {
        if (get_mode() == 0 && new_scanline)
        {
            // enable MODE 2: OAM search
            set_mode(2);
            // check if OAM interrupt enabled
            if (m_reg_stat & 0x20) io().trigger(lcd_stat);
        }
        else if (get_mode() == 2 && period >= oam_cycles())
        {
            // enable MODE 3: Scanline VRAM
            set_mode(3);

            // render a scanline (if rendering enabled)
            if (LIKELY(!this->m_state.white_frame && this->m_render))
            { this->render_scanline(m_state.current_scanline); }
            // TODO: perform HDMA transfers here!
        }
        else if (get_mode() == 3 && period >= oam_cycles() + vram_cycles())
        {
            // enable MODE 0: H-blank
            if (m_reg_stat & 0x8) io().trigger(lcd_stat);
            set_mode(0);
        }
        // printf("Current mode: %u -> %u period %lu\n",
        //        current_mode(), m_reg_stat & 0x3, period);
    }
}

bool GPU::is_vblank() const noexcept { return get_mode() == 1; }
bool GPU::is_hblank() const noexcept { return get_mode() == 0; }
uint8_t GPU::get_mode() const noexcept { return m_reg_stat & 0x3; }
void GPU::set_mode(uint8_t mode)
{
    this->m_reg_stat &= 0xfc;
    this->m_reg_stat |= mode & 0x3;
}

void GPU::do_ly_comparison()
{
    const bool equal = m_reg_ly == io().reg(IO::REG_LYC);
    // STAT coincidence bit
    setflag(equal, m_reg_stat, 0x4);
    // STAT interrupt (if enabled) when LY == LYC
    if (equal && (m_reg_stat & 0x40)) io().trigger(io().lcd_stat);
}

void GPU::render_frame()
{
    if (!m_state.white_frame && lcd_enabled())
    {
        // render each scanline
        for (int y = 0; y < SCREEN_H; y++) { this->render_scanline(y); }
    }
    else
    {
        // clear pixelbuffer with white
        std::fill_n(m_pixels.begin(), m_pixels.size(), WHITE_IDX);
    }
}

void GPU::render_scanline(int scan_y)
{
    const uint8_t scroll_y = memory().read8(IO::REG_SCY);
    const uint8_t scroll_x = memory().read8(IO::REG_SCX);
    const int sy = (scan_y + scroll_y) % 256;

    // create tiledata object from LCDC register
    auto td = this->create_tiledata(bg_tiles(), tile_data());
    // window visibility
    const bool window = this->window_visible() && scan_y >= window_y();
    auto wtd = this->create_tiledata(window_tiles(), tile_data());

    // create sprite configuration structure
    auto sprconf = this->sprite_config();
    sprconf.scan_y = scan_y;
    // create list of sprites that are on this scanline
    auto sprites = this->find_sprites(sprconf);

    // tile configuration
    tileconf_t tileconf = this->tile_config();

    // render whole scanline
    for (int scan_x = 0; scan_x < SCREEN_W; scan_x++)
    {
        const int sx = (scan_x + scroll_x) % 256;
        // get the tile id and attribute
        const int tid = td.tile_id(sx / 8, sy / 8);
        const int tattr = td.tile_attr(sx / 8, sy / 8);
        // copy the 16-byte tile into buffer
        const int tile_color = td.pattern(tid, tattr, sx & 7, sy & 7);
        uint16_t color15 = this->colorize_tile(tileconf, tattr, tile_color);

        if ((tattr & 0x80) == 0 || !machine().is_cgb())
        {
            // window on can be under sprites
            if (window && scan_x >= window_x() - 7)
            {
                const int wpx = scan_x - window_x() + 7;
                const int wpy = scan_y - window_y();
                // draw window pixel
                const int wtile = wtd.tile_id(wpx / 8, wpy / 8);
                const int wattr = wtd.tile_attr(wpx / 8, wpy / 8);
                const int widx = wtd.pattern(wtile, wattr, wpx & 7, wpy & 7);
                color15 = this->colorize_tile(tileconf, wattr, widx);
            }

            // render sprites within this x
            sprconf.scan_x = scan_x;
            for (const auto* sprite : sprites)
            {
                const uint8_t idx = sprite->pixel(sprconf);
                if (idx != 0)
                {
                    if (!sprite->behind() || tile_color == 0) {
						color15 = this->colorize_sprite(sprite, sprconf, idx);
					}
                }
            }
        } // BG priority
#ifndef GAMEBRO_INDEXED_FRAME
		// Convert to 15-bit RGB
		color15 = getpal(color15 * 2) | (getpal(color15 * 2 + 1) << 8);
        uint8_t r = ((color15) >> 0 & 0x1f) << 3;
        uint8_t g = ((color15) >> 5 & 0x1f) << 3;
        uint8_t b = ((color15) >> 10 & 0x1f) << 3;
		color15 = make_color(r,g,b);
#endif
        m_pixels.at(scan_y * SCREEN_W + scan_x) = color15;
    } // x
} // render_to(...)

uint16_t GPU::colorize_tile(const tileconf_t& conf, const uint8_t attr, const uint8_t idx)
{
    uint16_t index = 0;
    if (conf.is_cgb)
    {
        const uint8_t pal = attr & 0x7;
        index = 4 * pal + idx;
    }
    else
    {
        const uint8_t pal = conf.dmg_pal;
        index = (pal >> (idx * 2)) & 0x3;
    }
    // no conversion
    return index;
}
uint16_t GPU::colorize_sprite(const Sprite* sprite, sprite_config_t& sprconf, const uint8_t idx)
{
    uint16_t index = 0;
    if (machine().is_cgb()) {
		index = 32 + 4 * sprite->cgb_pal() + idx;
	}
    else {
        const uint8_t pal = sprconf.palette[sprite->pal()];
        index = (pal >> (idx * 2)) & 0x3;
    }
    // no conversion
    return index;
}

bool GPU::lcd_enabled() const noexcept { return m_reg_lcdc & 0x80; }
bool GPU::window_enabled() const noexcept { return m_reg_lcdc & 0x20; }
bool GPU::window_visible() { return window_enabled() && window_x() < 166 && window_y() < 143; }
int GPU::window_x() { return io().reg(IO::REG_WX); }
int GPU::window_y() { return io().reg(IO::REG_WY); }

uint16_t GPU::bg_tiles() const noexcept { return (m_reg_lcdc & 0x08) ? 0x9C00 : 0x9800; }
uint16_t GPU::window_tiles() const noexcept { return (m_reg_lcdc & 0x40) ? 0x9C00 : 0x9800; }
uint16_t GPU::tile_data() const noexcept { return (m_reg_lcdc & 0x10) ? 0x8000 : 0x8800; }

TileData GPU::create_tiledata(uint16_t tiles, uint16_t patterns)
{
    const bool is_signed = (m_reg_lcdc & 0x10) == 0;
    const auto* vram = memory().video_ram_ptr();
    // printf("Background tiles: 0x%04x  Tile data: 0x%04x\n",
    //        bg_tiles(), tile_data());
    const auto* tile_base = &vram[tiles - 0x8000];
    const auto* patt_base = &vram[patterns - 0x8000];
    const uint8_t* attr_base = nullptr;
    if (machine().is_cgb())
    {
        // attributes are always in VRAM bank 1 (which is off=0x2000)
        attr_base = &vram[tiles - 0x8000 + 0x2000];
    }
    return TileData{tile_base, patt_base, attr_base, is_signed};
}
tileconf_t GPU::tile_config()
{
    return tileconf_t{
        .is_cgb = machine().is_cgb(),
        .dmg_pal = memory().read8(IO::REG_BGP),
    };
}
sprite_config_t GPU::sprite_config()
{
    sprite_config_t config;
    config.patterns = memory().video_ram_ptr();
    config.palette[0] = memory().read8(IO::REG_OBP0);
    config.palette[1] = memory().read8(IO::REG_OBP1);
    config.scan_x = 0;
    config.scan_y = 0;
    config.set_height(m_reg_lcdc & 0x4);
    config.is_cgb = machine().is_cgb();
    return config;
}

std::vector<const Sprite*> GPU::find_sprites(const sprite_config_t& config) const
{
    const Sprite* sprite_begin = this->sprites_begin();
    const Sprite* sprite_back = this->sprites_end() - 1;
    std::vector<const Sprite*> results;
    // draw sprites from right to left
    for (const Sprite* sprite = sprite_back; sprite >= sprite_begin; sprite--)
    {
        if (sprite->hidden() == false)
            if (sprite->is_within_scanline(config))
            {
                results.push_back(sprite);
                // GB/GBC supports 10 sprites max per scanline
                if (results.size() == 10) break;
            }
    }
    return results;
}
const Sprite* GPU::sprites_begin() const noexcept { return &((Sprite*) memory().oam_ram_ptr())[0]; }
const Sprite* GPU::sprites_end() const noexcept { return &((Sprite*) memory().oam_ram_ptr())[40]; }

std::vector<uint16_t> GPU::dump_background()
{
    std::vector<uint16_t> data(256 * 256);
    // create tiledata object from LCDC register
    auto td = this->create_tiledata(bg_tiles(), tile_data());
    auto tconf = this->tile_config();

    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++)
        {
            // get the tile id
            const int tid = td.tile_id(x >> 3, y >> 3);
            const int tattr = td.tile_attr(x >> 3, y >> 3);
            // copy the 16-byte tile into buffer
            const int idx = td.pattern(tid, tattr, x & 7, y & 7);
            data.at(y * 256 + x) = this->colorize_tile(tconf, tattr, idx);
        }
    return data;
}
std::vector<uint16_t> GPU::dump_tiles(int bank)
{
    std::vector<uint16_t> data(16 * 24 * 8 * 8);
    // tiles start at the beginning of video RAM
    auto td = this->create_tiledata(0x8000, 0x8000);
    auto tconf = this->tile_config();
    const uint8_t attr = (bank == 0) ? 0x00 : 0x08;

    for (int y = 0; y < 24 * 8; y++)
        for (int x = 0; x < 16 * 8; x++)
        {
            int tile = (y / 8) * 16 + (x / 8);
            // copy the 16-byte tile into buffer
            const int idx = td.pattern(tile, attr, x & 7, y & 7);
            data.at(y * 128 + x) = this->colorize_tile(tconf, attr, idx);
        }
    return data;
}

void GPU::set_video_bank(const uint8_t bank)
{
    assert(bank < 2);
    this->m_state.video_offset = bank * 0x2000;
}
void GPU::lcd_power_changed(const bool online)
{
    // printf("Screen turned %s\n", online ? "ON" : "OFF");
    if (online)
    {
        // at the start of a new frame
        this->m_state.period = this->scanline_cycles();
        this->m_state.current_scanline = 153;
        this->m_reg_ly = this->m_state.current_scanline;
    }
    else
    {
        // LCD off, just reset to LY 0
        this->m_state.period = 0;
        this->m_state.current_scanline = 0;
        this->m_reg_ly = 0;
        // modify stat to V-blank?
        this->set_mode(1);
        // theres a full white frame when turning on again
        this->m_state.white_frame = true;
    }
}

void GPU::setpal(uint16_t index, uint8_t value)
{
    this->getpal(index) = value;
    // sprite palette index 0 is unused
    if (index >= 64 && (index & 7) < 2) return;
    //
    if (this->m_on_palchange)
    {
        const uint8_t base = index / 2;
        const uint16_t c16 = getpal(base * 2) | (getpal(base * 2 + 1) << 8);
        // linearize palette memory
        this->m_on_palchange(base, c16);
    }
} // setpal(...)

void GPU::set_dmg_variant(dmg_variant_t variant) { this->m_variant = variant; }

// serialization
int GPU::restore_state(const std::vector<uint8_t>& data, int off)
{
    this->m_state = *(state_t*) &data.at(off);
    return sizeof(m_state);
}
void GPU::serialize_state(std::vector<uint8_t>& res) const
{
    res.insert(res.end(), (uint8_t*) &m_state, (uint8_t*) &m_state + sizeof(m_state));
}
} // namespace gbc
