
#include "shiftrotate.h"
#include "bitutil.h"

#define C_BIT 4
#define C_MASK (1 << C_BIT)

#define Z_BIT 7
#define Z_MASK (1 << Z_BIT)

namespace gb
{
    static void setFlag(uint8_t& flags, uint8_t mask, bool set)
    {
        if (set)
        {
            setMask(flags, mask);
        }
        else
        {
            clearMask(flags, mask);
        }
    }

    uint8_t rlca(uint8_t val, uint8_t& flags)
    {
        uint8_t r = 0;

        uint8_t bit7 = (isBitSet(val, 7)) ? 1 : 0;

        r = (val << 1) | bit7;

        flags = 0; // clear N, Z and H
    //	flags |= (bit7 << C_BIT);
        setFlag(flags, C_MASK, bit7 == 1);

        return r;
    }

    uint8_t rla(uint8_t val, uint8_t& flags)
    {
        uint8_t r = 0;
        uint8_t c = (isBitSet(flags, C_BIT)) ? 1 : 0;

        uint8_t bit7 = (isBitSet(val, 7)) ? 1 : 0;

        r = (val << 1) | c;

        flags = 0;
        flags |= (bit7 << C_BIT);

        return r;
    }


    uint8_t rrca(uint8_t val, uint8_t& flags)
    {
        uint8_t r = 0;

        uint8_t bit0 = (isBitSet(val, 0)) ? 1 : 0;

        r = (val >> 1) | (bit0 << 7);

        flags = 0; // clear N, Z and H
        flags |= (bit0 << C_BIT);

        return r;
    }

    uint8_t rra(uint8_t val, uint8_t& flags)
    {
        uint8_t r = 0;
        uint8_t c = (isBitSet(flags, C_BIT)) ? 1 : 0;

        uint8_t bit0 = (isBitSet(val, 0)) ? 1 : 0;

        r = (val >> 1) | (c << 7);

        flags = 0;
        flags |= (bit0 << C_BIT);

        return r;
    }

    uint8_t rotateLeft(uint8_t val, uint8_t n, uint8_t& flags)
    {
        uint8_t r = 0;

        uint8_t bit7 = (isBitSet(val, 7)) ? 1 : 0;

        r = (val << n) | bit7;

        flags = 0; // clear N, Z and H
        //flags |= (bit7 << C_BIT);

        setFlag(flags, C_MASK, bit7 == 1);
        setFlag(flags, Z_MASK, r == 0);

        return r;
    }

    uint8_t rotateLeftCarry(uint8_t val, uint8_t n, uint8_t& flags)
    {
        uint8_t r = 0;
        uint8_t c = (isBitSet(flags, C_BIT)) ? 1 : 0;

        uint8_t bit7 = (isBitSet(val, 7)) ? 1 : 0;

        r = (val << n) | c;

        flags = 0;
        //flags |= (bit7 << C_BIT);

        setFlag(flags, C_MASK, bit7 == 1);
        setFlag(flags, Z_MASK, r == 0);

        return r;
    }

    uint8_t rotateRight(uint8_t val, uint8_t n, uint8_t& flags)
    {
        uint8_t r = 0;

        uint8_t bit0 = (isBitSet(val, 0)) ? 1 : 0;

        r = (val >> n) | (bit0 << 7);

        flags = 0; // clear N, Z and H
        //flags |= (bit0 << C_BIT);

        setFlag(flags, C_MASK, bit0 == 1);
        setFlag(flags, Z_MASK, r == 0);

        return r;
    }

    uint8_t rotateRightCarry(uint8_t val, uint8_t n, uint8_t& flags)
    {
        uint8_t r = 0;
        uint8_t c = (isBitSet(flags, C_BIT)) ? 1 : 0;

        uint8_t bit0 = (isBitSet(val, 0)) ? 1 : 0;

        r = (val >> n) | (c << 7);

        flags = 0;
        //flags |= (bit0 << C_BIT);

        setFlag(flags, C_MASK, bit0 == 1);
        setFlag(flags, Z_MASK, r == 0);

        return r;
    }

    uint8_t shiftLeft(uint8_t val, uint8_t n, uint8_t& flags)
    {
        uint8_t r = 0;
        uint8_t bit7 = (isBitSet(val, 7)) ? 1 : 0;

        r = (val << n);

        flags = 0;
        setFlag(flags, C_MASK, bit7 == 1);
        setFlag(flags, Z_MASK, r == 0);

        return r;
    }

    uint8_t shiftRightA(uint8_t val, uint8_t n, uint8_t& flags)
    {
        uint8_t r = 0;
        uint8_t bit0 = (isBitSet(val, 0)) ? 1 : 0;

        r = (val >> n) | (val & 0x80);

        flags = 0;
    //	flags |= (bit0 << C_BIT);

    //	if (r == 0) flags |= Z_MASK;

        setFlag(flags, C_MASK, bit0 == 1);
        setFlag(flags, Z_MASK, r == 0);

        return r;
    }

    uint8_t shiftRightL(uint8_t val, uint8_t n, uint8_t& flags)
    {
        uint8_t r = 0;
        uint8_t bit0 = (isBitSet(val, 0)) ? 1 : 0;

        r = (val >> n);

        flags = 0;
    //	flags |= (bit0 << C_BIT);

    //	if (r == 0) flags |= Z_MASK;

        setFlag(flags, C_MASK, bit0 == 1);
        setFlag(flags, Z_MASK, r == 0);

        return r;
    }

}
