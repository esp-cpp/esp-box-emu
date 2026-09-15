/*
 * Minimal replacement for SDL_endian.h used by TFE_System/endian.h.
 *
 * The Amiga port shipped a big-endian version of this shim (68k); the ESP32
 * (Xtensa) is little-endian like the Dark Forces data files, so every
 * "little endian to cpu" conversion is the identity and the "big endian to cpu"
 * conversions swap.
 */
#ifndef SDL_endian_h_
#define SDL_endian_h_

#include <stdint.h>
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;

#define SDL_LIL_ENDIAN  1234
#define SDL_BIG_ENDIAN  4321
#define SDL_BYTEORDER   SDL_LIL_ENDIAN
#define SDL_FORCE_INLINE inline
#define SDL_static_cast(x, y) ((x)(y))

#ifdef __cplusplus
extern "C" {
#endif

SDL_FORCE_INLINE Uint16
SDL_Swap16(Uint16 x)
{
    return SDL_static_cast(Uint16, ((x << 8) | (x >> 8)));
}

SDL_FORCE_INLINE Uint32
SDL_Swap32(Uint32 x)
{
    return SDL_static_cast(Uint32, ((x << 24) | ((x << 8) & 0x00FF0000) |
                                    ((x >> 8) & 0x0000FF00) | (x >> 24)));
}

SDL_FORCE_INLINE Uint64
SDL_Swap64(Uint64 x)
{
    Uint32 hi, lo;
    lo = SDL_static_cast(Uint32, x & 0xFFFFFFFF);
    x >>= 32;
    hi = SDL_static_cast(Uint32, x & 0xFFFFFFFF);
    x = SDL_Swap32(lo);
    x <<= 32;
    x |= SDL_Swap32(hi);
    return (x);
}

SDL_FORCE_INLINE float
SDL_SwapFloat(float x)
{
    union
    {
        float f;
        Uint32 ui32;
    } swapper;
    swapper.f = x;
    swapper.ui32 = SDL_Swap32(swapper.ui32);
    return swapper.f;
}

#define SDL_SwapLE16(X) (X)
#define SDL_SwapLE32(X) (X)
#define SDL_SwapLE64(X) (X)
#define SDL_SwapFloatLE(X)  (X)
#define SDL_SwapBE16(X) SDL_Swap16(X)
#define SDL_SwapBE32(X) SDL_Swap32(X)
#define SDL_SwapBE64(X) SDL_Swap64(X)
#define SDL_SwapFloatBE(X)  SDL_SwapFloat(X)

#ifdef __cplusplus
}
#endif

#endif
