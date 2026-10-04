#ifndef __MGS_LIBHZD_PRIVATE_H__
#define __MGS_LIBHZD_PRIVATE_H__

#include <sys/types.h>
#include <libgte.h>
#include "inline_n.h"
#include <gtemac.h>
#include "fmt_hzd.h"    // for HZD_VEC

#define	ZONE_HEIGHT (2000)

#define	MAX_ROUTE	(255)

#define CopyToHzdVec(dst, src)                  \
{                                               \
    ((HZD_VEC *)dst)->x = ((SVECTOR *)src)->vx; \
    ((HZD_VEC *)dst)->z = ((SVECTOR *)src)->vz; \
}

#define CopyFromHzdVec(dst, src)                \
{                                               \
    ((SVECTOR *)dst)->vx = ((HZD_VEC *)src)->x; \
    ((SVECTOR *)dst)->vz = ((HZD_VEC *)src)->z; \
}

static inline void Add2D(SVECTOR *out, SVECTOR *v1, SVECTOR *v2)
{
    out->vx = v1->vx + v2->vx;
    out->vy = v1->vy + v2->vy;
}

static inline void Sub2D(SVECTOR *out, SVECTOR *v1, SVECTOR *v2)
{
    out->vx = v1->vx - v2->vx;
    out->vy = v1->vy - v2->vy;
}

static inline void Mul2D(SVECTOR *out, SVECTOR *in, int num, int denom)
{
    out->vx = (in->vx * num) / denom;
    out->vy = (in->vy * num) / denom;
}

static inline long Dot2D(SVECTOR *v1, SVECTOR *v2)
{
    *(short *)(SCRPAD_ADDR + 0x004) = -v2->vy;
    *(short *)(SCRPAD_ADDR + 0x006) = v2->vx;
    gte_NormalClip(0, *(long *)v1, *(long *)(SCRPAD_ADDR + 0x004), (SCRPAD_ADDR + 0x008));
    return *(long *)(SCRPAD_ADDR + 0x008);
}

static inline long Det2D(SVECTOR *v1, SVECTOR *v2)
{
    gte_NormalClip(0, *(long *)v1, *(long *)v2, (SCRPAD_ADDR + 0x008));
    return *(long *)(SCRPAD_ADDR + 0x008);
}

static inline long Len2D(SVECTOR *vec)
{
    return SquareRoot0(Dot2D(vec, vec));
}

#endif // __MGS_LIBHZD_PRIVATE_H__
