#ifndef _COMMON_H_
#define _COMMON_H_

#include <og/sys/types.h>

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#ifndef NULL
#define NULL 0
#endif

#define LEN(x) ((s32)(sizeof(x) / sizeof(*(x))))
#define OFF(type, field) ((size_t)(&((type*)0)->field))
#ifdef PERMUTER
#define NOP
#else
#define NOP __asm("nop")
#endif
#define CLAMP(x, min, max) x < min ? min : (x > max ? max : x)

#ifndef PERMUTER
__asm__(".include \"macro_c.inc\"\n");
#define INCLUDE_ASM(FOLDER, NAME)                                              \
    __asm__(".pushsection .text\n"                                             \
            "\t.align\t2\n"                                                    \
            "\t.globl\t" #NAME "\n"                                            \
            "\t.ent\t" #NAME "\n" #NAME ":\n"                                  \
            ".include \"" FOLDER "/" #NAME ".s\"\n"                            \
            "\t.set reorder\n"                                                 \
            "\t.set at\n"                                                      \
            "\t.end\t" #NAME "\n"                                              \
            ".popsection")
#define WEAK_INCLUDE_ASM(FOLDER, NAME)                                         \
    __asm__(".pushsection .text\n"                                             \
            "\t.align\t2\n"                                                    \
            "\t.ent\t" #NAME "\n" #NAME ":\n"                                  \
            ".include \"" FOLDER "/" #NAME ".s\"\n"                            \
            "\t.set reorder\n"                                                 \
            "\t.set at\n"                                                      \
            "\t.end\t" #NAME "\n"                                              \
            ".popsection")
#define INCLUDE_RODATA(FOLDER, NAME)                                           \
    __asm__(".pushsection .rodata\n"                                           \
            ".include \"" FOLDER "/" #NAME ".s\"\n"                            \
            ".popsection")

#else // PERMUTER
#define INCLUDE_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)

#endif

#ifdef __psyz
#define O_TAG                                                                  \
    u_long tag;                                                                \
    u_long len
typedef struct {
    O_TAG;
} OT_TYPE;
#else
#define O_TAG u_long tag
#define OT_TYPE u_long
#endif

#endif
