#ifndef PSYZ_PLATFORM_H
#define PSYZ_PLATFORM_H

// Define PLATFORM_64BIT for 64-bit platforms
// Works with GCC, Clang, and MSVC
#if defined(__LP64__) || defined(_LP64) || defined(_WIN64) ||                  \
    defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__) ||       \
    defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM64) ||               \
    defined(__ia64__) || defined(__ppc64__) || defined(__powerpc64__)
#define PLATFORM_64BIT
#endif

#ifdef __psyz
#ifndef PLATFORM_PSYZ
#define PLATFORM_PSYZ
#endif
#endif

// Endianness: default to little-endian, switch to big only if a known
// BE-indicating macro is defined. Build targets (Linux x86_64 host, PSX
// MIPS) are both LE today; the BE branch is reserved for future ports.
// Honor an explicit -DPLATFORM_BE or -DPLATFORM_LE override too, so that
// either macro can be force-defined to sanity-check the BE codepath on
// an LE host without touching this header.
#if !defined(PLATFORM_LE) && !defined(PLATFORM_BE)
#if defined(__BIG_ENDIAN__) || defined(__BIG_ENDIAN) ||                        \
    defined(_BIG_ENDIAN) ||                                                    \
    (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) ||     \
    defined(_ARCH_PPC) || defined(__PPC__) || defined(__PPC) ||                \
    defined(__powerpc__) || defined(__powerpc)
#define PLATFORM_BE
#else
#define PLATFORM_LE
#endif
#endif

#endif
