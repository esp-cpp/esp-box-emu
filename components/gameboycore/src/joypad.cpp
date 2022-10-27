/**
    \author Natesh Narain <nnaraindev@gmail.com>
    \date Oct 8 2016
*/

#include "gameboycore/joypad.h"
#include "gameboycore/interrupt_provider.h"

namespace gb
{
    /* Private Implementation */

    class Joy::Impl
    {
    public:
        explicit Impl(MMU& mmu) :
            mmu_(mmu),
            reg_(mmu.get(memorymap::JOYPAD_REGISTER)),
            keys_(0xFF),
            interrupt_provider_(mmu, InterruptProvider::Interrupt::JOYPAD)
        {
            // add handlers
            mmu_.addReadHandler(memorymap::JOYPAD_REGISTER, std::bind(&Impl::readJoypad, this, std::placeholders::_1));
        }

        void press(Key key)
        {
            keys_ &= ~(1 << static_cast<uint8_t>(key));
            interrupt_provider_.set();
        }

        void release(Key key)
        {
            keys_ |= (1 << static_cast<uint8_t>(key));
        }

    private:
        uint8_t readJoypad(uint16_t) const
        {
            uint8_t hi = (reg_ & 0xF0);

            if ((hi & 0x30) == 0x10 || (hi & 0x30) == 0x20)
            {
                // first 2 bits of high nybble is group selection
                uint8_t group = ((~(reg_ >> 4)) & 0x03) - 1;

                uint8_t selection = (keys_ >> (group * 4)) & 0x0F;

                return (reg_ & 0xF0) | selection;
            }
            else
            {
                return reg_ | 0x0F;
            }
        }

    private:
        MMU& mmu_;
        uint8_t& reg_;
        uint8_t keys_;

        InterruptProvider interrupt_provider_;
    };
    

    /* Public Implementation */

    Joy::Joy(MMU& mmu) :
        impl_(new Impl(mmu))
    {

    }

    Joy::~Joy()
    {
        delete impl_;
    }

    void Joy::press(Key key)
    {
        impl_->press(key);
    }

    void Joy::release(Key key)
    {
        impl_->release(key);
    }
}