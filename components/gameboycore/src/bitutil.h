
/**
	Bit operation macros

	@author Natesh Narain
*/

#ifndef BITUTIL_H
#define BITUTIL_H

//! Bit value
template<typename T>
inline T bv(T b)
{
    return 1 << b;
}

//! set mask y in x
template<typename Tx,  typename Ty>
inline void setMask(Tx& x, Ty y) noexcept
{
    x |= (Tx)y;
}

//! clear mask y in x
template<typename Tx, typename Ty>
inline void clearMask(Tx& x, Ty y) noexcept
{
    x &= ~((Tx)y);
}
//! toggle mask y in x
template<typename Tx, typename Ty>
inline void toggleMask(Tx& x, Ty y) noexcept
{
    x ^= (Tx)y;
}

//! set bit y in x
template<typename Tx, typename Ty>
inline void setBit(Tx& x, Ty y) noexcept
{
    setMask(x, bv(y));
}
//! clear bit y in x
template<typename Tx, typename Ty>
inline void clearBit(Tx& x, Ty y) noexcept
{
    clearMask(x, bv(y));
}
//! toggle bit y in x
template<typename Tx, typename Ty>
inline void toggleBit(Tx& x, Ty y) noexcept
{
    toggleMask(x, bv(y));
}

//!
template<typename Tx>
inline Tx lownybble(const Tx& x) noexcept
{
    return x & 0x0F;
}

//!
template<typename T>
inline T highnybble(const T& t) noexcept
{
    return (t >> 4);
}

//! Create a WORD
template<typename Tx, typename Ty>
inline uint16_t word(const Tx& hi, const Ty& lo) noexcept
{
    return ((hi & 0xFFFF) << 8) | (lo & 0xFFFF);
}

//! Get bit
template<typename Tx, typename Ty>
inline Tx getBit(const Tx& x, const Ty& n) noexcept
{
    return !!(x & bv(n));
}

//! check if mask y is set in x
template<typename Tx, typename Ty>
inline bool isSet(const Tx& x, const Ty& y) noexcept
{
    return (x & y) != 0;
}

//! check if mask y is clear in x
template<typename Tx, typename Ty>
inline bool isClear(const Tx& x, const Ty& y) noexcept
{
    return !(x & y);
}
//! check if bit y is set in x
//#define isBitSet(x,y) ( isSet(x, bv(y)) )
template<typename Tx, typename Ty>
inline bool isBitSet(const Tx& x, const Ty& y) noexcept
{
    return isSet(x, bv(y));
}
//! check if bit y is clear in x
template<typename Tx, typename Ty>
inline bool isBitClear(const Tx& x, const Ty& y) noexcept
{
    return isClear(x, bv(y));
}

/* Full and Half Carry */

template<typename Tx, typename Ty>
inline bool isHalfCarry(const Tx& x, const Ty& y) noexcept
{
    return (((x & 0x0F) + (y & 0x0F)) & 0x10) != 0;
}

template<typename Tx, typename Ty>
inline bool isFullCarry(const Tx& x, const Ty& y) noexcept
{
    return (((x & 0x0FF) + (y & 0x0FF)) & 0x100) != 0;
}

template<typename Tx, typename Ty>
inline bool isHalfCarry16(const Tx& x, const Ty& y) noexcept
{
    return ((((x & 0x0FFF) + (y & 0x0FFF)) & 0x1000) != 0);
}

template<typename Tx, typename Ty>
inline bool isFullCarry16(const Tx& x, const Ty& y) noexcept
{
    return ((((x & 0x0FFFF) + ((y & 0x0FFFF))) & 0x10000) != 0);
}

template<typename Tx, typename Ty>
inline bool isHalfBorrow(const Tx& x, const Ty& y) noexcept
{
    return ((x & 0x0F) < (y & 0x0F));
}
template<typename Tx, typename Ty>
inline bool isFullBorrow(const Tx& x, const Ty& y) noexcept
{
    return ((x & 0xFF) < (y & 0xFF));
}

#endif // BITUTIL_H









