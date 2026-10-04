#ifndef PSYZ_TYPES_H
#define PSYZ_TYPES_H

#include <stdint.h>
#include <stddef.h>

// BSD-style unsigned types for PSX compatibility
#ifdef __psyz
#include <sys/types.h>

// sys/types.h may not define these on all platforms
#ifndef _BSD_SOURCE
typedef unsigned char u_char;
typedef unsigned short u_short;
#endif

// u_long* is widely used in PSY-Q, it should reflect the OS max pointer size
#ifdef _WIN64
// long on MSVC is 32-bit, unlike any other platform
typedef unsigned long long u_long;
#else
#ifndef _BSD_SOURCE
typedef unsigned long u_long;
#endif
#endif

#else

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;
#endif

typedef int8_t s8;
typedef int16_t s16;
#ifdef ESP_PLATFORM
// xtensa newlib defines int32_t as long; the game's own types.h says
// signed/unsigned int, and both headers meet in most translation units.
// Same width, different C identity - match the game.
typedef signed int s32;
typedef unsigned int u32;
#else
typedef int32_t s32;
typedef uint32_t u32;
#endif
typedef uint8_t u8;
typedef uint16_t u16;
typedef int8_t byte;

#ifndef NULL
#define NULL ((void*)0)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(
    sizeof(u_long) == sizeof(void*), "u_long type must be pointer aligned");
#endif

#endif
