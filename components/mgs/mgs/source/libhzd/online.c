#include "libhzd.h"
#include "private.h"

#include "common.h"
#include "libdg/libdg.h"
#include "libgv/libgv.h"
#include "inline_n.h"
#include "inline_x.h"
#include "game/map.h"   // for GM_IterHazard
#include "psxdefs.h"    // for getScratchAddr2

typedef struct SPAD_DATA
{
    char    pad[4];
    HZD_VEC vec[4];
} SPAD_DATA;

#define SPAD ((SPAD_DATA *)getScratchAddr(0))

STATIC int ComputeDirection(void)
{
    HZD_VEC *pVec1 = &SPAD->vec[3];
    HZD_VEC *pVec2 = &SPAD->vec[2];
    HZD_VEC *pVec3 = &SPAD->vec[1];
    int      area;

    pVec1->x = pVec2->x - pVec3->x;
    pVec1->y = pVec2->y - pVec3->y;
    pVec1->z = pVec2->z - pVec3->z;

    area = Len2D((SVECTOR *)(SCRPAD_ADDR + 0x01C));
    if (area == 0)
    {
        return 0;
    }

    pVec1->x = (pVec1->x * 256) / area;
    pVec1->y = (pVec1->y * 256) / area;
    pVec1->z = (pVec1->z * 256) / area;

    return area;
}

STATIC void ComputeBounds(SVECTOR *svec1, SVECTOR *svec2)
{
    SVECTOR *scratchvec1, *scratchvec2;
    int      coord1, coord2, coord1_copy;

    coord1 = svec1->vx;
    coord2 = svec2->vx;
    if (coord2 < coord1)
    {
        coord1_copy = coord1;
        coord1 = coord2;
        coord2 = coord1_copy;
    }
    scratchvec1 = (SVECTOR *)getScratchAddr(0x9);
    scratchvec2 = (SVECTOR *)getScratchAddr(0xB);

    scratchvec1->vx = coord1;
    scratchvec2->vx = coord2;

    coord1 = svec1->vz;
    coord2 = svec2->vz;
    if (coord2 < coord1)
    {
        coord1_copy = coord1;
        coord1 = coord2;
        coord2 = coord1_copy;
    }

    scratchvec1->vy = coord1;
    scratchvec2->vy = coord2;

    coord1 = svec1->vy;
    coord2 = svec2->vy;
    if (coord2 < coord1)
    {
        coord1_copy = coord1;
        coord1 = coord2;
        coord2 = coord1_copy;
    }

    scratchvec1->vz = coord1;
    scratchvec2->vz = coord2;
}

STATIC int CheckWallBounds(void)
{
    int z1, z2;
    int y1, y2;
    int cmp;

    if (getScratchAddr2(HZD_SEG, 0x34)->p1.x > getScratchAddr2(SVECTOR, 0x2C)->vx ||
        getScratchAddr2(HZD_SEG, 0x34)->p2.x < getScratchAddr2(SVECTOR, 0x24)->vx)
    {
        return 0;
    }

    z1 = getScratchAddr2(HZD_SEG, 0x34)->p1.z;
    z2 = getScratchAddr2(HZD_SEG, 0x34)->p2.z;

    if (z1 > z2)
    {
        SWAP( z1, z2 );
    }

    if (z1 > getScratchAddr2(SVECTOR, 0x2C)->vz || z2 < getScratchAddr2(SVECTOR, 0x24)->vz)
    {
        return 0;
    }

    y1 = getScratchAddr2(HZD_SEG, 0x34)->p1.y;
    y2 = getScratchAddr2(HZD_SEG, 0x34)->p2.y;

    cmp = getScratchAddr2(SVECTOR, 0x2C)->vy;
    if (y1 > cmp && y2 > cmp)
    {
        return 0;
    }

    y1 += getScratchAddr2(HZD_SEG, 0x34)->p1.h;
    y2 += getScratchAddr2(HZD_SEG, 0x34)->p2.h;

    cmp = getScratchAddr2(SVECTOR, 0x24)->vy;
    if (y1 < cmp && y2 < cmp)
    {
        return 0;
    }

    return 1;
}

STATIC int CalculateHitTime(void)
{
    long a;

    int opz_b;
    int opz_a;

    SVECTOR *ptr;
    SVECTOR *pa;
    SVECTOR *pb;

    // Can't get the code to generate a useless absolute load without this
#ifdef __psyz
    /* pinning a MIPS register only serves the matching build; elsewhere this is
     * an ordinary local */
    long *t0;
#else
    register long *t0 asm("t0");
#endif

    Sub2D((SVECTOR *)(SCRPAD_ADDR + 0x048), (SVECTOR *)(SCRPAD_ADDR + 0x03C), (SVECTOR *)(SCRPAD_ADDR + 0x034));

    a = *(long *)(SCRPAD_ADDR + 0x048);

    t0 = 0;
    gte_ldsxy3(t0, a, *(long *)(SCRPAD_ADDR + 0x01C));
    gte_nclip();

    ptr = (SVECTOR *)(SCRPAD_ADDR + 0x044);
    pa = (SVECTOR *)(SCRPAD_ADDR + 0x00C);
    pb = (SVECTOR *)(SCRPAD_ADDR + 0x034);

    Sub2D(ptr, pa, pb);
    ptr = 0;

    gte_read_opz(opz_a);

    t0 = (long *)(SCRPAD_ADDR + 0x044);
    opz_b = *t0;
    opz_a /= 16;

    asm("" :: "r"(t0));

    if (opz_a == 0)
    {
        return 0xF4240;
    }

    gte_ldsxy3(0 , opz_b, a);
    gte_nclip();
    gte_read_opz(opz_b);

    if (opz_b < 0)
    {
        opz_b = -opz_b;
        opz_a = -opz_a;
    }

    if (opz_b >= 0x9000000)
    {
        opz_a = HZD_DIV(opz_b, opz_a / 16);
    }
    else
    {
        opz_a = HZD_DIV(opz_b * 16, opz_a);
    }

    if (opz_a < 0)
    {
        return 0xF4240;
    }

    return opz_a;
}

STATIC int CalculateHitPoint(int mult)
{
    short  x, y, z;
    short *scratch1, *scratch2, *scratch3, *scratch4;

    scratch1 = (short *)(SCRPAD_ADDR + 0x01C);
    scratch2 = (short *)(SCRPAD_ADDR + 0x04C);
    scratch3 = (short *)(SCRPAD_ADDR + 0x00C);

    x = scratch2[0] = scratch3[0] + (scratch1[0] * mult) / 256;
    z = scratch2[2] = scratch3[2] + (scratch1[2] * mult) / 256;
    y = scratch2[1] = scratch3[1] + (scratch1[1] * mult) / 256;

    if (*(short *)(SCRPAD_ADDR + 0x048) != 0)
    {
        scratch4 = (short *)(SCRPAD_ADDR + 0x034);
        if (x < scratch4[0] - 32 || scratch4[4] + 32 < x)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        scratch4 = (short *)(SCRPAD_ADDR + 0x034);
        if (y < scratch4[1] - 32 || scratch4[5] + 32 < y)
        {
            return 0;
        }
        else
        {
            return 2;
        }
    }
}

STATIC void CalculateSegmentHeight(int a0)
{
    int v1;
    int v0;

    if (a0 == 1)
    {
        v0 = *(short *)(SCRPAD_ADDR + 0x04C);
        v1 = *(short *)(SCRPAD_ADDR + 0x034);
        v0 -= v1;
        v1 = *(short *)(SCRPAD_ADDR + 0x048);
    }
    else
    {
        v0 = *(short *)(SCRPAD_ADDR + 0x04E);
        v1 = *(short *)(SCRPAD_ADDR + 0x036);
        v0 -= v1;
        v1 = *(short *)(SCRPAD_ADDR + 0x04A);
    }

    v0 *= 4096;
    v1 = HZD_DIV(v0, v1);

    gte_lddp(v1);
    gte_ld_intpol_sv0((SVECTOR *)(SCRPAD_ADDR + 0x040));
    gte_ldopv2SV((SVECTOR *)(SCRPAD_ADDR + 0x038));
    gte_intpl();
    gte_stsv((SVECTOR *)(SCRPAD_ADDR + 0x038));
}

STATIC void TestSegment(HZD_SEG *seg, int a2, int a3)
{
    struct copier
    {
        int a, b;
    };

    short   *scratch1;
    HZD_SEG *scratch2;
    char    *scratch3;

    int      tmp1;
    int      tmp2;
    int      tmp3;
    int      tmp4;
    char    *tmp5;
    short    tmp6;

    *((HZD_SEG *)(SCRPAD_ADDR + 0x034)) = *seg;
    if (CheckWallBounds())
    {
        tmp1 = CalculateHitTime();
        tmp4 = CalculateHitPoint(tmp1);
        if (tmp4)
        {
            if (*(int *)(SCRPAD_ADDR + 0x060) < a2)
            {
                CalculateSegmentHeight(tmp4);
            }
            scratch1 = (short *)(SCRPAD_ADDR + 0x04C);
            scratch2 = (HZD_SEG *)(SCRPAD_ADDR + 0x034);
            tmp2 = scratch1[2] - scratch2->p1.y;
            if (tmp2 >= 0 && scratch2->p1.h >= tmp2)
            {
                *(int *)(SCRPAD_ADDR + 0x06C) += 1;
                if (*(int *)(SCRPAD_ADDR + 0x05C) >= tmp1)
                {
                    scratch3 = (char *)SCRPAD_ADDR;

                    *(struct copier *)(SCRPAD_ADDR + 0x054) = *(struct copier *)scratch1;
                    do {} while (0);

                    *(int *)(SCRPAD_ADDR + 0x05C) = tmp1;
                    tmp5 = *(char **)(scratch3 + 0x70);
                    tmp6 = *(short *)(scratch3 + 0x6A);
                    tmp4 = a3 & 127;
                    do
                    {
                    } while (0);

                    *(HZD_SEG **)(SCRPAD_ADDR + 0x064) = seg;
                    tmp3 = *(tmp5 - a2);
                    tmp3 <<= 8;
                    *(short *)(SCRPAD_ADDR + 0x068) = tmp6 | tmp4 | tmp3;
                }
            }
        }
    }
}

STATIC int HZD_80027BF8(SVECTOR *svec)
{
    int z;
    int y;
    int x;

    SVECTOR * scr = getScratchAddr2(SVECTOR, 0xC);

    x = svec->vx - scr->vx;
    if (x < 0)
    {
        x = -x;
    }

    z = svec->vz - scr->vz;
    if (z < 0)
    {
        z = -z;
    }

    x += z;

    y = svec->vy - scr->vy;
    if (y < 0)
    {
        y = -y;
    }

    return x + y;
}

STATIC int HZD_80027C64(void)
{
    int dividend;
    int val;

    val = *(short *)getScratchAddr(0x0E);

    if (val == *getScratchAddr(0x1D))
    {
        return *getScratchAddr(0x1E);
    }

    dividend = (val - *(short *)getScratchAddr(0x4)) * 4096;
    gte_lddp(HZD_DIV(dividend, *(short *)getScratchAddr(0x6) -
                               *(short *)getScratchAddr(0x4)));
    gte_ld_intpol_sv0((SVECTOR *)getScratchAddr(0x5));
    gte_ldopv2SV((SVECTOR *)getScratchAddr(0x3));
    gte_intpl();
    gte_stsv((SVECTOR *)getScratchAddr(0x13));
    *getScratchAddr(0x1D) = val;
    *getScratchAddr(0x1E) = HZD_80027BF8((SVECTOR *)getScratchAddr(0x13));
    return *getScratchAddr(0x1E);
}

STATIC int HZD_80027D80(HZD_FLR *floor)
{
    long  sxy_0;
    long  sxy_1;
    long  sxy_2;
    long  sxy_3;
    long  sxy_4;
    long *pZ;

    sxy_1 = *(long *)getScratchAddr(19);
    sxy_3 = floor->p1.long_access[0];
    sxy_0 = floor->p2.long_access[0];

    gte_ldsxy3(sxy_3, sxy_0, sxy_1);
    gte_nclip();
    sxy_2 = floor->p3.long_access[0];
    gte_stopz(getScratchAddr(2));

    pZ = (long *)getScratchAddr(2);

    if (*pZ >= 0)
    {
        gte_ldsxy3(sxy_0, sxy_2, sxy_1);
        gte_nclip();
        sxy_4 = floor->p4.long_access[0];
        gte_stopz(getScratchAddr(2));

        if (*pZ < 0)
        {
            return 0;
        }

        gte_NormalClip(sxy_2, sxy_4, sxy_1, getScratchAddr(2));
        if(*pZ < 0)
        {
            return 0;
        }

        gte_NormalClip(sxy_4, sxy_3, sxy_1, getScratchAddr(2));
        return *pZ >= 0;
    }
    else
    {
        gte_ldsxy3(sxy_0, sxy_2, sxy_1);
        gte_nclip();
        sxy_4 = floor->p4.long_access[0];
        gte_stopz(getScratchAddr(2));

        if (*pZ > 0)
        {
            return 0;
        }

        gte_NormalClip(sxy_2, sxy_4, sxy_1, getScratchAddr(2));
        if (*pZ > 0)
        {
            return 0;
        }

        gte_NormalClip(sxy_4, sxy_3, sxy_1, getScratchAddr(2));
        return *pZ <= 0;
    }
}

static inline void GetFloorHeight(SVECTOR *dst, HZD_FLR *a, HZD_VEC *b)
{
    dst->vx = a->p1.x - b->x;
    dst->vy = a->p1.y - b->y;
    dst->vz = a->p1.z - b->z;
}

static inline int GetScratch(int offset)
{
    int *ptr = (int *)SCRPAD_ADDR;
    return ptr[offset];
}

static inline void SetScratch(int offset, int value)
{
    int *ptr = (int *)SCRPAD_ADDR;
    ptr[offset] = value;
}

static inline int sub_helper_80027F10(void)
{
    if ((*(short *)(SCRPAD_ADDR + 0x036) > *(short *)(SCRPAD_ADDR + 0x030)) ||
        (*(short *)(SCRPAD_ADDR + 0x03E) < *(short *)(SCRPAD_ADDR + 0x028)) ||
        (*(short *)(SCRPAD_ADDR + 0x034) > *(short *)(SCRPAD_ADDR + 0x02C)) ||
        (*(short *)(SCRPAD_ADDR + 0x03C) < *(short *)(SCRPAD_ADDR + 0x024)) ||
        (*(short *)(SCRPAD_ADDR + 0x038) > *(short *)(SCRPAD_ADDR + 0x02E)) ||
        (*(short *)(SCRPAD_ADDR + 0x040) < *(short *)(SCRPAD_ADDR + 0x026)))
    {
        return 0;
    }

    return 1;
}

static inline int sub_helper2_80027F10(void)
{
    if ((*(short *)(SCRPAD_ADDR + 0x04C) < *(short *)(SCRPAD_ADDR + 0x034)) ||
        (*(short *)(SCRPAD_ADDR + 0x03C) < *(short *)(SCRPAD_ADDR + 0x04C)) ||
        (*(short *)(SCRPAD_ADDR + 0x04E) < *(short *)(SCRPAD_ADDR + 0x036)) ||
        (*(short *)(SCRPAD_ADDR + 0x03E) < *(short *)(SCRPAD_ADDR + 0x04E)))
    {
        return 0;
    }

    return 1;
}

//todo: include proper
#define UNTAG_PTR(_type, _ptr) (_type *)((unsigned int)_ptr & 0x7fffffff)

STATIC void TestFloor(HZD_FLR *floor)
{
    int flags;
    int length;
    int n, d;

    *(HZD_SEG *)(SCRPAD_ADDR + 0x034) = *(HZD_SEG *)floor;
    do {} while (0);

    if (!sub_helper_80027F10())
    {
        return;
    }

    flags = *(short *)(SCRPAD_ADDR + 0x03A);

    if ((flags & 2) != 0)
    {
        if (*(short *)(SCRPAD_ADDR + 0x010) == *(short *)(SCRPAD_ADDR + 0x018))
        {
            return;
        }

        length = HZD_80027C64();
    }
    else
    {
        if (GetScratch(0x23) == 0)
        {
            gte_ReadRotMatrix((SCRPAD_ADDR + 0x090));
            SetScratch(0x23, 1);
        }

        SetScratch(0x1F, floor->p1.h);
        SetScratch(0x20, floor->p3.h);
        SetScratch(0x21, floor->p2.h);
        gte_ldlvl((SCRPAD_ADDR + 0x07C));

        GetFloorHeight((SVECTOR *)(SCRPAD_ADDR + 0x0B0), (HZD_FLR *)(SCRPAD_ADDR + 0x004), (HZD_VEC *)(SCRPAD_ADDR + 0x00C));
        GetFloorHeight((SVECTOR *)(SCRPAD_ADDR + 0x0B6), floor, (HZD_VEC *)(SCRPAD_ADDR + 0x00C));

        gte_SetRotMatrix((SCRPAD_ADDR + 0x0B0));
        gte_rtir();
        gte_stlvnl((SCRPAD_ADDR + 0x07C));

        n = *(int *)(SCRPAD_ADDR + 0x080);
        d = *(int *)(SCRPAD_ADDR + 0x07C);

        if (((d < 0) && (n < 0)) || ((d > 0) && (n > 0)))
        {
            *(int *)(SCRPAD_ADDR + 0x074) = 0xF4240;
            *(short *)(SCRPAD_ADDR + 0x04C) = *(short *)(SCRPAD_ADDR + 0x00C) + (*(short *)(SCRPAD_ADDR + 0x0B0) * n) / d;
            *(short *)(SCRPAD_ADDR + 0x050) = *(short *)(SCRPAD_ADDR + 0x010) + (*(short *)(SCRPAD_ADDR + 0x0B2) * n) / d;
            *(short *)(SCRPAD_ADDR + 0x04E) = *(short *)(SCRPAD_ADDR + 0x00E) + (*(short *)(SCRPAD_ADDR + 0x0B4) * n) / d;

            length = HZD_80027BF8((SVECTOR *)(SCRPAD_ADDR + 0x04C));
        }
        else
        {
            length = 0xF4240;
        }
    }

    if (length >= *(int *)(SCRPAD_ADDR + 0x05C))
    {
        return;
    }

    if (!sub_helper2_80027F10())
    {
        return;
    }

    if ((flags & 1) || HZD_80027D80(floor))
    {
        *(int *)(SCRPAD_ADDR + 0x06C) += 1;
        *(HZD_VEC *)(SCRPAD_ADDR + 0x054) = *(HZD_VEC *)(SCRPAD_ADDR + 0x04C);
        *(int *)(SCRPAD_ADDR + 0x05C) = length;
        *(HZD_FLR **)(SCRPAD_ADDR + 0x064) = UNTAG_PTR(HZD_FLR, floor);
    }
}

static inline void CopySvector(SVECTOR *dst, SVECTOR *src)
{
    struct copy_struct
    {
        int a, b;
    };
    *(struct copy_struct *)dst = *(struct copy_struct *)src;
}

static inline void CopySvectorToSpad(int offset, SVECTOR *svec)
{
    short *spad_top;
    spad_top = (short *)SCRPAD_ADDR;

    spad_top[offset + 0] = svec->vx;
    spad_top[offset + 2] = svec->vy;
    spad_top[offset + 1] = svec->vz;
}

int HZD_OnlineHazardCheck(HZD_HDL *hzd, SVECTOR *from, SVECTOR *to, int chk_flag, int seg_flag)
{
    int       count;
    int       n_areas, n_areas2;
    int       bit1, bit2;
    HZD_GRP  *pArea;
    int       current_group;
    HZD_FLR  *pFloor;
    HZD_SEG  *pWall;
    HZD_FLR **ppFloor;
    HZD_SEG **ppWall;
    char     *pFlags;
    int       n_unknown;
    char     *pFlagsEnd;
    int       queue_size, idx;
    char     *pFlagsEnd2;
    HZD_HDL  *pNextMap;

    current_group = HZD_CurrentGroup;

    CopySvectorToSpad(6, from);

    *((int *)(SCRPAD_ADDR + 0x064)) = (*((int *)(SCRPAD_ADDR + 0x06C)) = 0);

    CopySvectorToSpad(10, to);
    CopySvector((SVECTOR *)(SCRPAD_ADDR + 0x054), (SVECTOR *)(SCRPAD_ADDR + 0x014));

    *((int *)(SCRPAD_ADDR + 0x08C)) = 0;

    ComputeBounds((SVECTOR *)(SCRPAD_ADDR + 0x00C), (SVECTOR *)(SCRPAD_ADDR + 0x054));

    *((int *)(SCRPAD_ADDR + 0x05C)) = ComputeDirection();

    if (!(*(int *)(SCRPAD_ADDR + 0x05C)))
    {
        return 0;
    }

    if (chk_flag & HZD_CHK_F_SEGMENT)
    {
        char *scratchpad;

        bit2 = 1;
        pArea = hzd->def->groups;
        for (n_areas2 = hzd->def->n_groups; n_areas2 > 0; n_areas2--, bit2 <<= 1, pArea++)
        {
            if (current_group & bit2)
            {
                do
                {
                    pWall = pArea->walls;
                    pFlags = pArea->wallsFlags;
                    do {} while (0);
                    n_unknown = pArea->n_flat_walls;
                    pFlagsEnd = pFlags + 2 * pArea->n_walls;
                    scratchpad = (char *)SCRPAD_ADDR;
                    *((short *)(scratchpad + 0x6A)) = 0;
                } while (0);

                *((char **)(scratchpad + 0x70)) = pFlagsEnd;
                *((int *)(SCRPAD_ADDR + 0x060)) = n_unknown;

                for (count = pArea->n_walls; count > 0; count--, pWall++, pFlags++)
                {
                    if (!((*pFlags) & seg_flag))
                    {
                        TestSegment(pWall, count, *pFlags);
                    }
                }
            }
        }
    }

    if (chk_flag & HZD_CHK_D_SEGMENT)
    {
        char *scratchpad;

        pNextMap = NULL;
        while ((pNextMap = GM_IterHazard(pNextMap)))
        {
            scratchpad = (char *)SCRPAD_ADDR;
            do
            {
                ppWall = pNextMap->dynamic_segments;
                pFlags = pNextMap->dynamic_flags;
                queue_size = pNextMap->max_dynamic_segments;
                idx = pNextMap->dynamic_queue_index;
                *((short *)(scratchpad + 0x6A)) = 0x80;
                do
                {
                } while (0);

                pFlagsEnd2 = (pFlags + queue_size) + idx;
                *((char **)(scratchpad + 0x70)) = pFlagsEnd2;
            } while (0); // TODO: Is it the same macro as above in "if (chk_flag & HZD_CHK_F_SEGMENT)" case?

            count = pNextMap->dynamic_queue_index;
            *((int *)(SCRPAD_ADDR + 0x060)) = 0;

            for (; count > 0; count--, ppWall++, pFlags++)
            {
                if (!((*pFlags) & seg_flag))
                {
                    TestSegment(*ppWall, count, *pFlags);
                }
            }
        }
    }
    ComputeBounds((SVECTOR *)(SCRPAD_ADDR + 0x00C), (SVECTOR *)(SCRPAD_ADDR + 0x054));
    *((int *)(SCRPAD_ADDR + 0x05C)) = HZD_80027BF8((SVECTOR *)(SCRPAD_ADDR + 0x054));
    *((int *)(SCRPAD_ADDR + 0x074)) = 0xF4240;

    if (chk_flag & HZD_CHK_F_FLOOR)
    {
        bit1 = 1;
        pArea = hzd->def->groups;
        for (n_areas = hzd->def->n_groups; n_areas > 0; n_areas--, bit1 <<= 1, pArea++)
        {
            if (current_group & bit1)
            {
                pFloor = pArea->floors;
                for (count = pArea->n_floors; count > 0; count--)
                {
                    TestFloor(pFloor);
                    pFloor++;
                }
            }
        }
    }

    if (chk_flag & HZD_CHK_D_FLOOR)
    {
        pNextMap = NULL;
        while ((pNextMap = GM_IterHazard(pNextMap)))
        {
            ppFloor = pNextMap->dynamic_floors;
            for (count = pNextMap->dynamic_floor_index; count > 0; count--, ppFloor++)
            {
                TestFloor(*ppFloor);
            }
        }
    }

    if (*(int *)(SCRPAD_ADDR + 0x08C) != 0)
    {
        gte_SetRotMatrix((SCRPAD_ADDR + 0x090));
    }

    if (*(int *)(SCRPAD_ADDR + 0x064) != 0)
    {
        return *(int *)(SCRPAD_ADDR + 0x06C);
    }
    return 0;
}

void *HZD_GetOnlineHazard(void)
{
    return *getScratchAddr2(void **, 0x64);
}

int HZD_GetOnlineHazardAtr(void)
{
    return *getScratchAddr2(short, 0x68);
}

void HZD_GetOnlineVector(SVECTOR *vect_ptr)
{
    HZD_VEC *cross;
    HZD_VEC *from;

    cross = getScratchAddr2(HZD_VEC, 0x54);
    from = getScratchAddr2(HZD_VEC, 0x0c);

    vect_ptr->vx = cross->x - from->x;
    vect_ptr->vy = cross->y - from->y;
    vect_ptr->vz = cross->z - from->z;
}

void HZD_GetOnlinePoint(SVECTOR *ptp_ptr)
{
    HZD_VEC *cross;

    cross = getScratchAddr2(HZD_VEC, 0x54);

    ptp_ptr->vx = cross->x;
    ptp_ptr->vy = cross->y;
    ptp_ptr->vz = cross->z;
}
