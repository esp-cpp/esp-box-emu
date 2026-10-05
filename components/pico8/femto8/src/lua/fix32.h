/*
 * fix32.h - Pure C 16.16 fixed-point arithmetic
 *
 * Port of z8::fix32 from femto8 (C++ operator overloads -> C inline functions).
 * All PICO-8 numbers are stored as int32_t with 16 integer bits and 16 fractional bits.
 */

#ifndef FIX32_H
#define FIX32_H

#include <stdint.h>
#include <math.h>

typedef int32_t fix32_t;

/* Constants */
#define FIX32_ZERO      ((fix32_t)0)
#define FIX32_ONE       ((fix32_t)0x00010000)
#define FIX32_HALF      ((fix32_t)0x00008000)
#define FIX32_NEG_ONE   ((fix32_t)0xFFFF0000)
#define FIX32_MAX       ((fix32_t)0x7FFFFFFF)
#define FIX32_MIN       ((fix32_t)0x80000001)  /* not 0x80000000, per PICO-8 */

/* Construction */
static inline fix32_t fix32_from_int(int x)       { return (fix32_t)x << 16; }
static inline fix32_t fix32_from_double(double d)  { return (fix32_t)(int64_t)(d * 65536.0); }
static inline fix32_t fix32_from_bits(int32_t b)   { return b; }

/* Conversion */
static inline int     fix32_to_int(fix32_t x)      { return x >> 16; }
static inline double  fix32_to_double(fix32_t x)    { return (double)x / 65536.0; }
static inline int32_t fix32_bits(fix32_t x)         { return x; }

/* Comparisons */
static inline int fix32_eq(fix32_t a, fix32_t b)  { return a == b; }
static inline int fix32_ne(fix32_t a, fix32_t b)  { return a != b; }
static inline int fix32_lt(fix32_t a, fix32_t b)  { return a < b; }
static inline int fix32_gt(fix32_t a, fix32_t b)  { return a > b; }
static inline int fix32_le(fix32_t a, fix32_t b)  { return a <= b; }
static inline int fix32_ge(fix32_t a, fix32_t b)  { return a >= b; }
static inline int fix32_bool(fix32_t a)           { return a != 0; }

/* Basic arithmetic - use unsigned to avoid signed overflow UB */
static inline fix32_t fix32_add(fix32_t a, fix32_t b) { return (fix32_t)((uint32_t)a + (uint32_t)b); }
static inline fix32_t fix32_sub(fix32_t a, fix32_t b) { return (fix32_t)((uint32_t)a - (uint32_t)b); }
static inline fix32_t fix32_neg(fix32_t a)             { return (fix32_t)(-(uint32_t)a); }

static inline fix32_t fix32_mul(fix32_t a, fix32_t b) {
    return (fix32_t)((int64_t)a * b >> 16);
}

static inline fix32_t fix32_div(fix32_t a, fix32_t b) {
    /* Special case: 0x8000/0x1 = 0x8000, not 0x8000.0001 */
    if (b == FIX32_ONE)
        return a;

    if (b) {
        int64_t result = (int64_t)a * 0x10000 / b;
        int64_t abs_result = result < 0 ? -result : result;
        if (abs_result <= (int64_t)0x7FFFFFFFU)
            return (fix32_t)result;
    }

    /* Return 0x8000.0001 (not 0x8000.0000) for -Inf, just like PICO-8 */
    return (a ^ b) >= 0 ? (fix32_t)0x7FFFFFFFU : (fix32_t)0x80000001U;
}

static inline fix32_t fix32_mod(fix32_t a, fix32_t b) {
    /* PICO-8 always returns positive values */
    fix32_t abs_b = b < 0 ? -b : b;
    /* PICO-8 0.2.5f: x % 0 gives 0 */
    if (!abs_b) return 0;
    int32_t result = a % abs_b;
    return result >= 0 ? result : result + abs_b;
}

static inline fix32_t fix32_idiv(fix32_t a, fix32_t b) {
    fix32_t d = fix32_div(a, b);
    return d & (fix32_t)0xFFFF0000;  /* floor = mask off fraction */
}

/* Bitwise */
static inline fix32_t fix32_band(fix32_t a, fix32_t b) { return a & b; }
static inline fix32_t fix32_bor(fix32_t a, fix32_t b)  { return a | b; }
static inline fix32_t fix32_bxor(fix32_t a, fix32_t b) { return a ^ b; }
static inline fix32_t fix32_bnot(fix32_t a)             { return ~a; }

/* Forward declaration for mutual recursion */
static inline fix32_t fix32_lshr(fix32_t x, int y);

static inline fix32_t fix32_shl(fix32_t x, int y) {
    /* Negative y = lshr instead */
    if (y < 0) return fix32_lshr(x, -y);
    return y >= 32 ? 0 : x << y;
}

static inline fix32_t fix32_shr(fix32_t x, int y) {
    /* Arithmetic right shift. Negative y = left shift */
    if (y < 0) return fix32_shl(x, -y);
    int shift = y < 31 ? y : 31;
    return x >> shift;
}

static inline fix32_t fix32_lshr(fix32_t x, int y) {
    /* Logical (unsigned) right shift. Negative y = left shift */
    if (y < 0) return fix32_shl(x, -y);
    return y >= 32 ? 0 : (fix32_t)((uint32_t)x >> y);
}

static inline fix32_t fix32_rotl(fix32_t x, int y) {
    y &= 0x1f;
    return (x << y) | (fix32_t)((uint32_t)x >> (32 - y));
}

static inline fix32_t fix32_rotr(fix32_t x, int y) {
    y &= 0x1f;
    return (fix32_t)((uint32_t)x >> y) | (x << (32 - y));
}

/* Math functions */

/* PICO-8 0.2.3: abs(0x8000) should be 0x7fff.ffff */
static inline fix32_t fix32_abs(fix32_t a) {
    if (a >= 0) return a;
    if ((a << 1) == 0) return ~a;  /* 0x80000000 -> 0x7FFFFFFF */
    return -a;
}

static inline fix32_t fix32_min(fix32_t a, fix32_t b) { return a < b ? a : b; }
static inline fix32_t fix32_max(fix32_t a, fix32_t b) { return a > b ? a : b; }

static inline fix32_t fix32_mid(fix32_t a, fix32_t b, fix32_t c) {
    if (a > b) { fix32_t t = a; a = b; b = t; }
    /* now a <= b */
    if (c <= a) return a;
    if (c >= b) return b;
    return c;
}

static inline fix32_t fix32_flr(fix32_t x) {
    return x & (fix32_t)0xFFFF0000;
}

static inline fix32_t fix32_ceil(fix32_t x) {
    return fix32_neg(fix32_flr(fix32_neg(x)));
}

static inline fix32_t fix32_sgn(fix32_t x) {
    if (x > 0) return FIX32_ONE;
    if (x < 0) return FIX32_NEG_ONE;
    return FIX32_ONE;  /* PICO-8: sgn(0) = 1 */
}

static inline fix32_t fix32_pow(fix32_t x, fix32_t y) {
    return fix32_from_double(pow(fix32_to_double(x), fix32_to_double(y)));
}

#endif /* FIX32_H */
