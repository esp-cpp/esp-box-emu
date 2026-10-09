#include <psyz.h>
#include <libgte.h>
#include <psyz/log.h>
#include "../internal.h"

extern short SQRT[192];

// https://github.com/OpenDriver2/PsyCross/blob/093501/src/gte/PsyX_GTE.cpp#L51
// modified fast version to handle negative values, borrowed from PsyX GTE impl
static unsigned int gte_leadingzerocount(unsigned int lzcs) {
    if (!lzcs) {
        return 32;
    }
    if (lzcs & 0x80000000) {
        lzcs = ~lzcs;
    }
    unsigned int lzcr = lzcs;
    static char debruijn32[32] = {
        0, 31, 9, 30, 3, 8,  13, 29, 2,  5,  7,  21, 12, 24, 28, 19,
        1, 10, 4, 14, 6, 22, 25, 20, 11, 15, 23, 26, 16, 27, 17, 18};

    lzcr |= lzcr >> 1;
    lzcr |= lzcr >> 2;
    lzcr |= lzcr >> 4;
    lzcr |= lzcr >> 8;
    lzcr |= lzcr >> 16;
    lzcr++;
    return debruijn32[lzcr * 0x076be629 >> 27];
}

long SquareRoot0_impl(long a) {
    unsigned int lzc;
    long shift;
    long result_shift;
    long table_shift;
    long normalized;

    if (a == 0) {
        return 0;
    }
    if (a < 0) {
        /* Callers pass a sum of squares, so negative means the caller's own
         * arithmetic went wrong upstream -- there is no meaningful root to
         * return. The code below would carry on regardless: a negative
         * normalizes to an index outside the table and the wrap-around picks
         * an arbitrary entry, handing back a large plausible-looking distance
         * that then propagates into whatever the caller does with it. Zero is
         * the honest answer and keeps the damage local to the frame that
         * produced it. */
        WARNF("SquareRoot0(%d) negative value", (int)a);
        return 0;
    }

    lzc = gte_leadingzerocount(a);
    if (lzc == 0x20) {
        return 0;
    }

    shift = lzc & ~1; // Make even
    table_shift = shift - 0x18;
    if (table_shift >= 0) {
        normalized = a << table_shift;
    } else {
        normalized = a >> (0x18 - shift);
    }

    normalized -= 0x40;
    if (normalized < 0 || normalized >= LEN(SQRT)) {
        WARNF("SquareRoot0(%d) out of bounds", (int)a);
        normalized = ((normalized % 192) + 192) % 192;
    }
    result_shift = (0x1F - shift) >> 1;
    return (long)(SQRT[normalized] << result_shift) >> 12;
}

long SquareRoot12_impl(long a) {
    unsigned int lzc;
    long shift;
    long result_shift;
    long table_shift;
    long normalized;

    if (a == 0) {
        return 0;
    }
    if (a < 0) {
        WARNF("SquareRoot12(%d) negative value", (int)a);
    }

    lzc = gte_leadingzerocount(a);
    if (lzc == 0x20) {
        return 0;
    }

    shift = lzc & ~1; // Make even
    table_shift = shift - 0x18;
    if (table_shift >= 0) {
        normalized = a << table_shift;
    } else {
        normalized = a >> (0x18 - shift);
    }

    normalized -= 0x40;
    if (normalized < 0 || normalized >= LEN(SQRT)) {
        WARNF("SquareRoot12(%d) out of bounds", (int)a);
        normalized = ((normalized % 192) + 192) % 192;
    }
    result_shift = (0x1F - shift) >> 1;
    return (long)(SQRT[normalized] << result_shift) >> 6;
}
