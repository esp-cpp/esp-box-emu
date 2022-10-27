/**
    \file gpu.h
    \brief GPU emulation
    \author Natesh Narain <nnaraindev@gmail.com>
    \date Oct 23 2016

    \defgroup Graphics
*/

#ifndef GAMEBOYCORE_GPU_H
#define GAMEBOYCORE_GPU_H

#include "gameboycore_api.h"
#include "gameboycore/mmu.h"
#include "gameboycore/pixel.h"
#include "gameboycore/sprite.h"

#include <memory>
#include <cstdint>
#include <functional>
#include <array>
#include <vector>

namespace gb
{
    /**
        \class GPU
        \brief Handle LCD state, compute scanlines and send to an external renderer
        \ingroup API
        \ingroup Graphics
    */
    class GAMEBOYCORE_API GPU
    {
    public:
        //! Smart pointer type
        using Ptr = std::unique_ptr<GPU>;

        //! Array on Pixel objects representing a single scan line produced by the GPU
        using Scanline = std::array<Pixel, 160>;

        /**
            Callback function called by the GPU when it has produced a new scan line
            Provides the Scanline and the line number
        */
        using RenderScanlineCallback = std::function<void(const Scanline&, int linenum)>;

        /**
            Callback function call by the GPU when VBlank is reached
        */
        using VBlankCallback = std::function<void()>;

    public:
        explicit GPU(MMU::Ptr& mmu);
        GPU(const GPU&) = delete;
        ~GPU();

        /**
            Update the GPU with elasped cycles. Used by the CPU
        */
        void update(uint8_t cycles, bool ime);

        /**
            Set the host system callback
        */
        void setRenderCallback(RenderScanlineCallback callback);

        /**
            Set the host system vblank callback
        */
        void setVBlankCallback(VBlankCallback callback);

        /**
            Set Default Palette Color
        */
        void setPaletteColor(uint8_t r, uint8_t g, uint8_t b, int idx);

        /**
            \return Background tilemap data
        */
        std::vector<uint8_t> getBackgroundTileMap();

        /**
            \return currently cached tile data
        */
        std::array<Sprite, 40> getSpriteCache() const;

        /**
            \return Hashed background map
        */
        std::size_t getBackgroundHash();

    private:
        //! Private Implementation class
        class Impl;
        Impl* impl_;
    };
}

#endif
