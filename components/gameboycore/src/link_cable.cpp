#include "gameboycore/link_cable.h"

namespace gb
{
    /* Private Implementation */

    class LinkCable::Impl
    {
    public:
        //! Contains link ready status, byte to transfer and clocking mode
        struct LinkData
        {
            LinkData() :
                mode(Link::Mode::EXTERNAL),
                byte(0),
                ready(false)
            {
            }

            Link::Mode mode;
            uint8_t    byte;
            bool       ready;
        };

        void link1ReadyCallback(uint8_t byte, Link::Mode mode)
        {
            link_data1_.byte = byte;
            link_data1_.mode = mode;
            link_data1_.ready = true;

            update();
        }

        void link2ReadyCallback(uint8_t byte, Link::Mode mode)
        {
            link_data2_.byte = byte;
            link_data2_.mode = mode;
            link_data2_.ready = true;

            update();
        }

        void setLink1RecieveCallback(const RecieveCallback& callback)
        {
            recieve1_ = callback;
        }

        void setLink2RecieveCallback(const RecieveCallback& callback)
        {
            recieve2_ = callback;
        }

    private:
        void update()
        {
            if (link_data1_.ready && link_data2_.ready)
            {
                // both links indicate they are ready to transfer

                if (link_data1_.mode != link_data2_.mode)
                {
                    transfer();
                }
            }
            else
            {
                // only one link is ready to tranfer, or neither are


                if (link_data1_.ready)
                {
                    if (link_data1_.mode == Link::Mode::INTERNAL)
                    {
                        // if this link is the master device, supply it with a $FF
                        link_data2_.byte = 0xFF;
                        transfer();
                    }
                }

                if (link_data2_.ready)
                {
                    if (link_data2_.mode == Link::Mode::INTERNAL)
                    {
                        link_data1_.byte = 0xFF;
                        transfer();
                    }
                }

            }
        }

        void transfer()
        {
            if (recieve1_)
                recieve1_(link_data2_.byte);

            if (recieve2_)
                recieve2_(link_data1_.byte);

            link_data1_.ready = false;
            link_data2_.ready = false;
        }

    private:
        RecieveCallback recieve1_;
        RecieveCallback recieve2_;

        LinkData link_data1_;
        LinkData link_data2_;
    };

    /* Public Implementation */

    LinkCable::LinkCable() :
        impl_(new Impl)
    {
    }

    LinkCable::~LinkCable()
    {
        delete impl_;
    }

    void LinkCable::link1ReadyCallback(uint8_t byte, Link::Mode mode)
    {
        impl_->link1ReadyCallback(byte, mode);
    }

    void LinkCable::link2ReadyCallback(uint8_t byte, Link::Mode mode)
    {
        impl_->link2ReadyCallback(byte, mode);
    }

    void LinkCable::setLink1RecieveCallback(const RecieveCallback& callback)
    {
        impl_->setLink1RecieveCallback(callback);
    }

    void LinkCable::setLink2RecieveCallback(const RecieveCallback& callback)
    {
        impl_->setLink2RecieveCallback(callback);
    }

}