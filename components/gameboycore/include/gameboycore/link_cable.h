/**
    \file link_cable.h
    \brief Gameboy Link Cable Emulation
    \author Natesh Narain <nnaraindev@clearpath.ai>
    \date Dec 10 2016
*/

#ifndef GAMEBOYCORE_LINK_CABLE_H
#define GAMEBOYCORE_LINK_CABLE_H

#include "gameboycore/gameboycore_api.h"
#include "gameboycore/link.h"

#include <cstdint>
#include <functional>

namespace gb
{
    /**
        \class Link Cable
        \brief Contains Gameboy link cable logic
        \ingroup API
    */
    class GAMEBOYCORE_API LinkCable
    {
    public:
        //! Callback from Link Cable
        using RecieveCallback = std::function<void(uint8_t)>;

        LinkCable();
        ~LinkCable();

        void link1ReadyCallback(uint8_t byte, Link::Mode mode);
        void link2ReadyCallback(uint8_t byte, Link::Mode mode);

        void setLink1RecieveCallback(const RecieveCallback& callback);
        void setLink2RecieveCallback(const RecieveCallback& callback);

    private:
        class Impl;
        Impl* impl_;
    };
}

#endif // GAMEBOYCORE_LINK_CABLE_H
