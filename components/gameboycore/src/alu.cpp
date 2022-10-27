
#include "gameboycore/alu.h"

#include "bitutil.h"

namespace gb
{
    ALU::ALU(uint8_t& flags) :
        flags_(flags)
    {
    }

    void ALU::add(uint8_t& a, uint8_t n)
    {
        bool is_half_carry = isHalfCarry(a, n);
        bool is_full_carry = isFullCarry(a, n);

        a += n;

        setFlag(ALU::Flags::H, is_half_carry);
        setFlag(ALU::Flags::C, is_full_carry);
        setFlag(ALU::Flags::Z, (a == 0));
        setFlag(ALU::Flags::N, false);
    }

    void ALU::add(uint16_t& hl, uint16_t n)
    {
        bool is_half_carry = isHalfCarry16(hl, n);
        bool is_full_carry = isFullCarry16(hl, n);

        hl += n;

        setFlag(ALU::Flags::H, is_half_carry);
        setFlag(ALU::Flags::C, is_full_carry);
        setFlag(ALU::Flags::N, false);
    }

    void ALU::addr(uint16_t& sp, int8_t n)
    {
        int result = sp + n;
            
        setFlag(Flags::C, ((sp ^ n ^ (result & 0xFFFF)) & 0x100) == 0x100);
        setFlag(Flags::H, ((sp ^ n ^ (result & 0xFFFF)) & 0x10) == 0x10);

        sp = (uint16_t)result;

        setFlag(ALU::Flags::Z, false);
        setFlag(ALU::Flags::N, false);
    }

    void ALU::addc(uint8_t& a, uint8_t n)
    {
        int carry = (isSet(flags_, ALU::Flags::C)) ? 1 : 0;

        int result = (int)a + (int)n + carry;

        setFlag(ALU::Flags::H, ((a & 0x0F) + (n & 0x0F) + carry) > 0x0F);
        setFlag(ALU::Flags::C, result > 0xFF);
        setFlag(ALU::Flags::Z, ((uint8_t)result == 0));
        setFlag(ALU::Flags::N, false);

        a = (uint8_t)result;
    }

    void ALU::sub(uint8_t& a, uint8_t n)
    {
        bool is_half_borrow = isHalfBorrow(a, n);
        bool is_full_borrow = isFullBorrow(a, n);

        a -= n;

        setFlag(ALU::Flags::H, is_half_borrow);
        setFlag(ALU::Flags::C, is_full_borrow);
        setFlag(ALU::Flags::Z, (a == 0));
        setFlag(ALU::Flags::N, true);
    }

    void ALU::subc(uint8_t& a, uint8_t n)
    {
        int carry = (isSet(flags_, ALU::Flags::C)) ? 1 : 0;
        int result = (int)a - n - carry;

        setFlag(ALU::Flags::C, result < 0);
        setFlag(ALU::Flags::H, ((a & 0x0F) - (n & 0x0F) - carry) < 0);

        a = (uint8_t)result;

        setFlag(ALU::Flags::Z, (a == 0));
        setFlag(ALU::Flags::N, true);
    }

    void ALU::anda(uint8_t& a, uint8_t n)
    {
        a &= n;

        setFlag(ALU::Flags::N, false);
        setFlag(ALU::Flags::H, true);
        setFlag(ALU::Flags::C, false);
        setFlag(ALU::Flags::Z, (a == 0));
    }

    void ALU::ora(uint8_t& a, uint8_t n)
    {
        a |= n;

        setFlag(ALU::Flags::N, false);
        setFlag(ALU::Flags::Z, (a == 0));
        setFlag(ALU::Flags::H, false);
        setFlag(ALU::Flags::C, false);
    }

    void ALU::xora(uint8_t& a, uint8_t n)
    {
        a ^= n;

        setFlag(ALU::Flags::Z, (a == 0));
        setFlag(ALU::Flags::N, false);
        setFlag(ALU::Flags::H, false);
        setFlag(ALU::Flags::C, false);
    }

    void ALU::compare(uint8_t& a, uint8_t n)
    {
        bool is_half_borrow = isHalfBorrow(a, n);
        bool is_full_borrow = isFullBorrow(a, n);

        uint8_t r = a - n;

        setFlag(ALU::Flags::H, is_half_borrow);
        setFlag(ALU::Flags::C, is_full_borrow);
        setFlag(ALU::Flags::Z, (r == 0));
        setFlag(ALU::Flags::N, true);
    }

    void ALU::setFlag(uint8_t mask, bool set)
    {
        if (set)
        {
            setMask(flags_, mask);
        }
        else
        {
            clearMask(flags_, mask);
        }
    }

    ALU::~ALU()
    {
    }
}
