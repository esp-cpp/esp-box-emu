
/**
    \file joypad.h
    \brief Emulate Gameboy user input
    \author Natesh Narain <nnaraindev@gmail.com>
*/

#ifndef GAMEBOYCORE_JOYPAD_H
#define GAMEBOYCORE_JOYPAD_H

#include "gameboycore/gameboycore_api.h"
#include "gameboycore/mmu.h"

#include <functional>
#include <memory>

namespace gb
{
    /**
        \class Joy
        \brief Emulate Gameboy Joypad
        \ingroup API
    */
    class GAMEBOYCORE_API Joy
    {
    public:
        //! Keys on the Gameboy
        enum class Key
        {
            RIGHT  = 0,
            LEFT   = 1,
            UP     = 2,
            DOWN   = 3,
            A      = 4,
            B      = 5,
            SELECT = 6,
            START  = 7
        };

        //! Smart pointer type
        using Ptr = std::unique_ptr<Joy>;

        explicit Joy(MMU& mmu);
        Joy(const Joy&);
        ~Joy();

        /**
            Press Key on the Gameboy
        */
        void press(Key key);
        /**
            Release Key on the Gameboy
        */
        void release(Key key);

    private:
        //! Private Implementation
        class Impl;
        Impl* impl_;
    };
}

#endif // GAMEBOY_JOYPAD_H
