#include "gameboycore/link.h"
#include "gameboycore/memorymap.h"
#include "gameboycore/interrupt_provider.h"

#include "bitutil.h"

#include <queue>
#include <iostream> // TODO: remove

namespace gb
{
    /* Private Interface */

    class Link::Impl
    {
    public:

        explicit Impl(MMU::Ptr& mmu) : 
            control_(mmu->get(memorymap::SC_REGISTER)),
            byte_to_transfer_(0),
            byte_to_recieve_(0),
            serial_interrupt_{ *mmu.get(), InterruptProvider::Interrupt::SERIAL },
            shift_clock_(0),
            shift_counter_(0),
            shift_clock_rate_(0),
            pending_recieve_(false)
        {
            // serial byte handlers
            mmu->addReadHandler(memorymap::SB_REGISTER, std::bind(&Impl::recieveHandler, this, std::placeholders::_1));
            mmu->addWriteHandler(memorymap::SB_REGISTER, std::bind(&Impl::sendHandler, this, std::placeholders::_1, std::placeholders::_2));

            // control callback
            mmu->addWriteHandler(memorymap::SC_REGISTER, std::bind(&Impl::control, this, std::placeholders::_1, std::placeholders::_2));
        }

        ~Impl()
        {
        }

        void update(uint8_t cycles)
        {	
            if (!isTransferring() || pending_recieve_) return;

            // if using internal shift clock, run clocking logic
            if (getLinkMode() == Link::Mode::INTERNAL)
            {
                internalClock(cycles);
            }
            else
            {
                // transferring in external clock mode, signal transfer ready
                
                signalReady();
            }
        }

        void internalClock(uint8_t cycles)
        {
            // increment shift clock
            shift_clock_ += cycles;

            if (shift_clock_ >= shift_clock_rate_)
            {
                shift_clock_ -= shift_clock_rate_;

                shift_counter_++;

                if (shift_counter_ == 8)
                {
                    // signal to the host system that this core is ready to do the transfer
                    signalReady();

                    shift_counter_ = 0;
                }
            }
        }

        void control(uint8_t value, uint16_t) noexcept
        {
            control_ = (control_ & 0x80) | 0x02 | value;

            shift_clock_rate_ = getTransferRate(value);

            pending_recieve_ = false;
        }

        void sendHandler(uint8_t value, uint16_t) noexcept
        {
            byte_to_transfer_ = value;
        }

        uint8_t recieveHandler(uint16_t) const noexcept
        {
            return byte_to_recieve_;
        }

        /**
            Data into the core
        */
        void recieve(uint8_t byte)
        {
            // recieve the byte
            byte_to_recieve_ = byte;
            // set serial interrupt
            serial_interrupt_.set();
            // clear transfer flag
            clearMask(control_, memorymap::SC::TRANSFER);
            
            pending_recieve_ = false;
        }

        void setReadyCallback(const ReadyCallback& callback)
        {
            ready_callback_ = callback;
        }

    private:

        bool isTransferring() const noexcept
        {
            return isSet(control_, memorymap::SC::TRANSFER) != 0;
        }

        int getTransferRate(uint8_t sc)
        {
            // TODO: CGB speed modes
            (void)sc;
            return 4194304 / 8192;
        }

        void signalReady()
        {
            if (ready_callback_) 
            {
                ready_callback_(byte_to_transfer_, getLinkMode());
                pending_recieve_ = true;
            }
        }

        Mode getLinkMode() const noexcept
        {
            if (isSet(control_, memorymap::SC::CLOCK_MODE))
            {
                return Mode::INTERNAL;
            }
            else
            {
                return Mode::EXTERNAL;
            }
        }

    private:
        //! Serial Control Register
        uint8_t& control_;

        //! Byte to be transfered to the opponent gameboy
        uint8_t byte_to_transfer_;
        //! Byte recieved by the opponent gameboy
        uint8_t byte_to_recieve_;

        //! ready callback
        ReadyCallback ready_callback_;

        //! Serial interrupt provider
        InterruptProvider serial_interrupt_;

        //! Internal Timer
        int shift_clock_;
        //! Count the shift clock overflows
        int shift_counter_;
        //! Transfer rate
        int shift_clock_rate_;
        
        //! Flag indicating that the link port is waiting to recieve a byte
        bool pending_recieve_;
    };

    /* Public Interface */

    Link::Link(MMU::Ptr& mmu) :
        impl_(new Impl(mmu))
    {
    }

    void Link::update(uint8_t cycles)
    {
        impl_->update(cycles);
    }

    void Link::recieve(uint8_t byte)
    {
        impl_->recieve(byte);
    }

    void Link::setReadyCallback(const ReadyCallback& callback)
    {
        impl_->setReadyCallback(callback);
    }

    Link::~Link()
    {
        delete impl_;
    }
}