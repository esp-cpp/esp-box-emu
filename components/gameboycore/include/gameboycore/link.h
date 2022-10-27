/**
    \file link.h
    \brief Link port emulation
    \author Natesh Narain <nnaraindev@gmail.com>
    \date Nov 30 2016
*/

#ifndef GAMEBOYCORE_LINK_H
#define GAMEBOYCORE_LINK_H

#include "gameboycore/gameboycore_api.h"
#include "gameboycore/mmu.h"

#include <cstdint>
#include <memory>
#include <functional>

namespace gb
{
    /**
        \class Link
        \brief Emulate link port
        \ingroup API
    */
    class GAMEBOYCORE_API Link
    {
    public:
        enum class Mode
        {
            INTERNAL, ///< Internal Clock Mode
            EXTERNAL  ///< External Clock Mode
        };

        //! Smart pointer type
        using Ptr = std::unique_ptr<Link>;

        //! Callback to signal transfer ready status
        using ReadyCallback = std::function<void(uint8_t, Mode)>;

    public:
        explicit Link(MMU::Ptr& mmu);
        Link(const Link&) = delete;
        ~Link();

        /**
            Update with CPU cycles elapsed
        */
        void update(uint8_t cycles);

        /**
            Write a byte buffer into the core
        */
        void recieve(uint8_t byte);

        /**
            Signal that this core is ready to transfer a byte
        */
        void setReadyCallback(const ReadyCallback& callback);

    private:
        class Impl;
        Impl* impl_;
    };
}

#endif
