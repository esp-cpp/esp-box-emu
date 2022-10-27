#include "gameboycore/gpu.h"
#include "gameboycore/palette.h"
#include "gameboycore/memorymap.h"
#include "gameboycore/pixel.h"
#include "gameboycore/interrupt_provider.h"
#include "gameboycore/tilemap.h"

#include "gameboycore/detail/hash.h"

#include "bitutil.h"

namespace gb
{
    static constexpr auto HBLANK_CYCLES = 207;
    static constexpr auto OAM_ACCESS_CYCLES = 83;
    static constexpr auto LCD_TRANSFER_CYCLES = 175;

    static constexpr auto LINE_CYCLES = 456;
    static constexpr auto VBLANK_LINE = 144;
    static constexpr auto MAX_LINES = 153;

    /* Private Implementation */

    class GPU::Impl
    {
    public:
        enum class Mode
        {
            HBLANK,
            VBLANK,
            OAM,
            LCD
        };

        /**
            Data needed for HDMA transfer
        */
        struct Hdma
        {
            Hdma() :
                transfer_active(false),
                source(0),
                destination(0),
                length(0)
            {
            }

            bool transfer_active;
            uint16_t source;
            uint16_t destination;
            uint16_t length;
        };

        using CgbPalette = std::array<std::array<gb::Pixel, 4>, 8>;

        explicit Impl(MMU::Ptr& mmu) :
            mmu_(mmu),
            mode_(Mode::OAM),
            cycle_count_(0),
            line_(0),
            lcdc_(mmu->get(memorymap::LCDC_REGISTER)),
            stat_(mmu->get(memorymap::LCD_STAT_REGISTER)),
            hdma5_(mmu->get(memorymap::HDMA5)),
            vblank_provider_(*mmu.get(), InterruptProvider::Interrupt::VBLANK),
            stat_provider_(*mmu.get(), InterruptProvider::Interrupt::LCDSTAT),
            tilemap_(*mmu.get(), palette_),
            cgb_enabled_(mmu->cgbEnabled())
        {
            mmu->addWriteHandler(memorymap::LCDC_REGISTER, std::bind(&Impl::lcdcWriteHandler, this, std::placeholders::_1, std::placeholders::_2));
            mmu->addWriteHandler(memorymap::BGPD, std::bind(&Impl::paletteWriteHandler, this, std::placeholders::_1, std::placeholders::_2));
            mmu->addWriteHandler(memorymap::OBPD, std::bind(&Impl::paletteWriteHandler, this, std::placeholders::_1, std::placeholders::_2));
            mmu->addWriteHandler(memorymap::HDMA5, std::bind(&Impl::hdma5WriteHandler, this, std::placeholders::_1, std::placeholders::_2));
        }

        void update(uint8_t cycles, bool ime)
        {
            if (isClear(lcdc_, memorymap::LCDC::ENABLE)) return;

            cycle_count_ += cycles;

            switch (mode_)
            {
                case Mode::HBLANK:
                    // check if the HBLANK period is over
                    if (hasElapsed(HBLANK_CYCLES))
                    {
                        // update the scan line
                        updateLY();
                        // check if LY matches LYC
                        compareLyToLyc(ime);

                        // check if in VBlank mode
                        if (line_ == VBLANK_LINE)
                        {
                            mode_ = Mode::VBLANK;
                            vblank_provider_.set();

                            if (vblank_callback_)
                                vblank_callback_();
                        }
                        else
                        {
                            mode_ = Mode::OAM;
                        }

                        checkStatInterrupts(ime);
                    }
                    break;
                case Mode::OAM:
                    if (hasElapsed(OAM_ACCESS_CYCLES))
                    {
                        mode_ = Mode::LCD;
                    }
                    break;
                case Mode::LCD:
                    if (hasElapsed(LCD_TRANSFER_CYCLES))
                    {
                        // render the current scan line
                        renderScanline();

                        mode_ = Mode::HBLANK;
                        // perform an hdma transfer
                        doHdma();
                        checkStatInterrupts(ime);
                    }
                    break;
                case Mode::VBLANK:
                    if (hasElapsed(LINE_CYCLES))
                    {
                        updateLY();
                        compareLyToLyc(ime);

                        if (line_ == 0)
                        {
                            mode_ = Mode::OAM;
                            checkStatInterrupts(ime);
                        }
                    }
                    break;
            }

            // update LCDC stat mode
            stat_ = (stat_ & 0xFC) | (static_cast<uint8_t>(mode_));
        }

        void setRenderCallback(RenderScanlineCallback callback)
        {
            render_scanline_ = callback;
        }

        void setVBlankCallback(VBlankCallback callback)
        {
            vblank_callback_ = callback;
        }

        void setDefaultPaletteColor(uint8_t r, uint8_t g, uint8_t b, int idx)
        {
            palette_.set(r, g, b, idx);
        }

        std::vector<uint8_t> getBackgroundTileMap()
        {
            return tilemap_.getBackgroundTileMap();
        }

        std::array<Sprite, 40> getSpriteCache() const
        {
            return tilemap_.getSpriteCache();
        }

        std::size_t getBackgroundHash()
        {
            return tilemap_.hashBackground();
        }

    private:

        void renderScanline()
        {
            Scanline scanline;
            std::array<uint8_t, 160> color_line;

            auto background_palette = palette_.get(mmu_->read(memorymap::BGP_REGISTER));

            // get lcd config

            const auto background_enabled = isSet(lcdc_, memorymap::LCDC::BG_DISPLAY_ON) != 0;
            const auto window_enabled     = isSet(lcdc_, memorymap::LCDC::WINDOW_ON)     != 0;
            const auto sprites_enabled    = isSet(lcdc_, memorymap::LCDC::OBJ_ON)        != 0;

            // get background tile line
            const auto background = tilemap_.getBackground(line_, cgb_enabled_);

            // get window overlay tile line
            const auto window = tilemap_.getWindowOverlay(line_);
            const auto wx = mmu_->read(memorymap::WX_REGISTER);
            const auto wy = mmu_->read(memorymap::WY_REGISTER);

            // compute a scan line
            for (auto pixel_idx = 0u; pixel_idx < scanline.size(); ++pixel_idx)
            {
                auto tileinfo = 0u;

                if (window_enabled && line_ >= (int)wy && (int)pixel_idx >= (wx - 7))
                    tileinfo = window[pixel_idx];
                else if (background_enabled || cgb_enabled_)
                    tileinfo = background[pixel_idx];
                else
                    tileinfo = 0;

                auto color_number = tileinfo & 0x03;
                auto color_palette = (tileinfo >> 2) & 0x07;
                auto priority = (tileinfo >> 5);

                color_line[pixel_idx] = (uint8_t)(color_number | (priority << 2));

                if (cgb_enabled_)
                {
                    scanline[pixel_idx] = cgb_background_palettes_[color_palette][color_number];
                }
                else
                {
                    scanline[pixel_idx] = background_palette[color_number];
                }
            }

            if (sprites_enabled)
                tilemap_.drawSprites(scanline, color_line, line_, cgb_enabled_, cgb_sprite_palette_);

            // send scan line to the renderer
            if (render_scanline_ && line_ < VBLANK_LINE)
                render_scanline_(scanline, line_);
        }

        bool hasElapsed(int mode_cycles)
        {
            if (cycle_count_ >= mode_cycles)
            {
                cycle_count_ -= mode_cycles;
                return true;
            }
            return false;
        }

        void updateLY()
        {
            line_ = (line_ + 1) % MAX_LINES;
            mmu_->write((uint8_t)line_, memorymap::LY_REGISTER);
        }

        void lcdcWriteHandler(uint8_t value, uint16_t)
        {
            bool enable = (value & memorymap::LCDC::ENABLE) != 0;

            if (enable && isClear(lcdc_, memorymap::LCDC::ENABLE))
            {
                line_ = 0;
                cycle_count_ = 0;
            }

            lcdc_ = value;
        }

        void paletteWriteHandler(uint8_t value, uint16_t addr)
        {
            if (addr == memorymap::BGPD)
            {
                setPalette(cgb_background_palettes_, value, memorymap::BGPI);
            }
            else if(addr == memorymap::OBPD)
            {
                setPalette(cgb_sprite_palette_, value, memorymap::OBPI);
            }
        }

        void setPalette(CgbPalette& palettes, uint8_t value, uint16_t index_reg)
        {
            // get the background palette index
            const auto index = mmu_->read(index_reg);

            // extract high byte, color index and palette index info from background palette index
            auto hi = index & 0x01;
            auto color_idx = (index >> 1) & 0x03;
            auto palette_idx = (index >> 3) & 0x07;

            // RGB value break down
            // MSB: | xBBBBBGG |
            // LBS: | GGGRRRRR |

            auto& palette_color = palettes[palette_idx][color_idx];

            if (hi)
            {
                palette_color.b = (value >> 2) & 0x1F;
                palette_color.g |= ((value & 0x03) << 3);

                palette_color = translateRGB(palette_color);
            }
            else
            {
                palette_color.g = ((value & 0xE0) >> 5);
                palette_color.r = (value & 0x1F);
            }

            // auto increment index if increment flag is set
            if (isBitSet(index, 7))
            {
                mmu_->write((uint8_t)(index + 1), index_reg);
            }
        }

        Pixel translateRGB(const Pixel& pixel)
        {
            Pixel out;
            out.r = scale(pixel.r);
            out.g = scale(pixel.g);
            out.b = scale(pixel.b);

            return out;
        }

        uint8_t scale(uint8_t v)
        {
            auto old_range = (0x1F - 0x00);
            auto new_range = (0xFF - 0x00);

            return (uint8_t)((v * new_range) / old_range);
        }

        void hdma5WriteHandler(uint8_t value, uint16_t)
        {
            uint16_t src = word(mmu_->read(memorymap::HDMA1), mmu_->read(memorymap::HDMA2)) & 0xFFF0;
            uint16_t dest = word(((mmu_->read(memorymap::HDMA3) & 0x1F) | 0x80), mmu_->read(memorymap::HDMA4)) & 0xFFF0;
            uint16_t length = ((value & 0x7F) + 1) * 0x10;

            if (isBitClear(value, 7) && !hdma_.transfer_active)
            {
                // perform a general purpose DMA
                mmu_->dma(dest, src, length);
            }
            else if (isBitClear(value, 7) && hdma_.transfer_active)
            {
                // disable an active hdma transfer
                hdma_.transfer_active = false;
            }
            else
            {
                // initialize an HDMA transfer
                hdma_.source = src;
                hdma_.destination = dest;
                hdma_.length = length;
                hdma_.transfer_active = true;
            }

            hdma5_ = value;
        }

        void doHdma()
        {
            if (hdma_.transfer_active)
            {
                // hdma only works between this range
                if (line_ >= 0 && line_ <= 143)
                {
                    // transfer $10 bytes
                    mmu_->dma(hdma_.destination, hdma_.source, 0x10);
                    // advance source $10 bytes
                    hdma_.source += 0x10;
                    // advance destination $10 bytes
                    hdma_.destination += 0x10;
                    // count down the length
                    hdma_.length -= 0x10;

                    if (hdma_.length == 0)
                    {
                        hdma_.transfer_active = false;
                    }
                }
            }
        }

        void compareLyToLyc(bool ime)
        {
            auto lyc = mmu_->read(memorymap::LYC_REGISTER);

            if ((uint8_t)line_ == lyc)
            {
                //stat_ |= memorymap::Stat::LYCLY;
                setMask(stat_, memorymap::Stat::LYCLY);
            }
            else
            {
                //stat_ &= ~(memorymap::Stat::LYCLY);
                clearMask(stat_, memorymap::Stat::LYCLY);
            }

            // check the ly=lyc flag
            if (stat_ & memorymap::Stat::LYCLY)
            {
                if (ime)
                    stat_provider_.set();
            }
        }

        void checkStatInterrupts(bool ime)
        {
            // check mode selection interrupts
            uint8_t mask = (1 << (3 + static_cast<uint8_t>(mode_)));

            if (stat_ & mask)
            {
                if (ime)
                    stat_provider_.set();
            }
        }

    private:
        MMU::Ptr& mmu_;

        Mode mode_;
        int cycle_count_;
        int line_;
        uint8_t& lcdc_;
        uint8_t& stat_;
        uint8_t& hdma5_;
        Hdma hdma_;

        InterruptProvider vblank_provider_;
        InterruptProvider stat_provider_;

        detail::TileMap tilemap_;
        Palette palette_;

        RenderScanlineCallback render_scanline_;
        VBlankCallback vblank_callback_;

        bool cgb_enabled_;

        CgbPalette cgb_background_palettes_;
        CgbPalette cgb_sprite_palette_;
    };

    /* Public Implementation */

    GPU::GPU(MMU::Ptr& mmu) :
        impl_(new Impl(mmu))
    {
    }

    GPU::~GPU()
    {
        delete impl_;
    }

    void GPU::update(uint8_t cycles, bool ime)
    {
        impl_->update(cycles, ime);
    }

    void GPU::setRenderCallback(RenderScanlineCallback callback)
    {
        impl_->setRenderCallback(callback);
    }

    void GPU::setVBlankCallback(VBlankCallback callback)
    {
        impl_->setVBlankCallback(callback);
    }

    void GPU::setPaletteColor(uint8_t r, uint8_t g, uint8_t b, int idx)
    {
        impl_->setDefaultPaletteColor(r, g, b, idx);
    }

    std::vector<uint8_t> GPU::getBackgroundTileMap()
    {
        return impl_->getBackgroundTileMap();
    }

    std::array<Sprite, 40> GPU::getSpriteCache() const
    {
        return impl_->getSpriteCache();
    }

    std::size_t GPU::getBackgroundHash()
    {
        return impl_->getBackgroundHash();
    }

} // namespace gb
