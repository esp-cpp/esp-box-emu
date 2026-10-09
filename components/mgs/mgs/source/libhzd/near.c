#include "libhzd.h"
#include "private.h"

#include "common.h"
#include "inline_n.h"
#include "inline_x.h"
#include "psxdefs.h"    // for getScratchAddr2
#include "libdg/libdg.h"

static void CopyVector(SVECTOR *src, HZD_VEC *dst)
{
    dst->x = src->vx;
    dst->y = src->vy;
    dst->z = src->vz;
}

static void CreateBoundingBox(HZD_VEC *vec, int range)
{
    SVECTOR *min;
    SVECTOR *max;
    int      comp;

    min = (SVECTOR *)(SCRPAD_ADDR + 0x14);
    max = (SVECTOR *)(SCRPAD_ADDR + 0x1C);

    comp = vec->x;
    min->vx = comp - range;
    max->vx = comp + range;

    comp = vec->z;
    min->vz = comp - range;
    max->vz = comp + range;

    comp = vec->y;
    max->vy = comp;
    min->vy = comp;
}

STATIC int HZD_80028930(void)
{
    int   lzcnt;
    int   num;
    int   opz, opz2, opz3;

    short *ptr1;
    short *ptr2;

    Sub2D((SVECTOR *)(SCRPAD_ADDR + 0x038), (SVECTOR *)(SCRPAD_ADDR + 0x02C), (SVECTOR *)(SCRPAD_ADDR + 0x024));
    Sub2D((SVECTOR *)(SCRPAD_ADDR + 0x034), (SVECTOR *)(SCRPAD_ADDR + 0x00C), (SVECTOR *)(SCRPAD_ADDR + 0x024));

    opz = Dot2D((SVECTOR *)(SCRPAD_ADDR + 0x038), (SVECTOR *)(SCRPAD_ADDR + 0x034));

    *(int *)(SCRPAD_ADDR + 0x04C) = 1;
    *(int *)(SCRPAD_ADDR + 0x0AC) = 1;

    if (opz < 0)
    {
        *(int *)(SCRPAD_ADDR + 0x0A8) = 0;
        Sub2D((SVECTOR *)(SCRPAD_ADDR + 0x05C), (SVECTOR *)(SCRPAD_ADDR + 0x024), (SVECTOR *)(SCRPAD_ADDR + 0x00C));
    }
    else
    {
        opz2 = Dot2D((SVECTOR *)(SCRPAD_ADDR + 0x038), (SVECTOR *)(SCRPAD_ADDR + 0x038));

        if (opz2 < opz)
        {
            *(int *)(SCRPAD_ADDR + 0x0A8) = 1;
            Sub2D((SVECTOR *)(SCRPAD_ADDR + 0x05C), (SVECTOR *)(SCRPAD_ADDR + 0x02C), (SVECTOR *)(SCRPAD_ADDR + 0x00C));
        }
        else
        {
            opz3 = Det2D((SVECTOR *)(SCRPAD_ADDR + 0x038), (SVECTOR *)(SCRPAD_ADDR + 0x034));

            gte_ldlzc(opz2);
            gte_stlzc((SCRPAD_ADDR + 0x0A4));

            lzcnt = 16 - *(int *)(SCRPAD_ADDR + 0x0A4);

            if (lzcnt > 0)
            {
                opz >>= lzcnt;
                opz3 >>= lzcnt;
                opz2 >>= lzcnt;
            }

            *(int *)(SCRPAD_ADDR + 0x0A8) = opz;
            *(int *)(SCRPAD_ADDR + 0x0AC) = opz2;

            num = *(short *)(SCRPAD_ADDR + 0x03A) * opz3;

            ptr1 = (short *)(SCRPAD_ADDR + 0x04C);
            /* opz2 is a squared 2D length and reaches zero when the point sits
             * exactly on a segment endpoint, or when the segment is degenerate
             * in the XZ plane. MIPS shrugs: R3000 divide-by-zero leaves the
             * result undefined and execution carries on, so on the console
             * this frame just produced a meaningless normal and the next one
             * was fine. Xtensa raises IntegerDivideByZero and takes the board
             * down -- which is the reboot seen while walking Snake around the
             * dock. Answer zero and let the caller's own "was it zero?" fixup
             * below pick the sign, which is the same shape of recovery the
             * original code already applies. */
            ptr1[8] = opz2 ? (short)(num / opz2) : 0;

            if ((ptr1[8] == 0) && (num != 0))
            {
                ptr1[8] = (num > 0) ? 1 : -1;
            }

            num = -*(short *)(SCRPAD_ADDR + 0x038) * opz3;

            ptr2 = (short*)(SCRPAD_ADDR + 0x04C);
            ptr2[9] = opz2 ? (short)(num / opz2) : 0;

            if ((ptr2[9] == 0) && (num != 0))
            {
                ptr2[9] = (num > 0) ? 1 : -1;
            }

            *(int *)(SCRPAD_ADDR + 0x04C) = 0;
            *(int *)(SCRPAD_ADDR + 0x060) = *(int *)(SCRPAD_ADDR + 0x024);
            *(int *)(SCRPAD_ADDR + 0x064) = *(int *)(SCRPAD_ADDR + 0x02C);
        }
    }

    *(int *)(SCRPAD_ADDR + 0x050) = Dot2D((SVECTOR *)(SCRPAD_ADDR + 0x05C), (SVECTOR *)(SCRPAD_ADDR + 0x05C));
    return *(int *)(SCRPAD_ADDR + 0x050);
}

STATIC void HZD_80028CF8(void)
{
    /* same divisor as HZD_80028930 stored at 0x0AC, same reason it can be
     * zero, and the same fatal difference on this CPU */
    {
        int den = *(int *)(SCRPAD_ADDR + 0x0AC);
        gte_lddp(den ? (*(int *)(SCRPAD_ADDR + 0x0A8) * 4096) / den : 0);
    }
    gte_ld_intpol_sv0((SVECTOR *)(SCRPAD_ADDR + 0x030));
    gte_ldopv2SV((SVECTOR *)(SCRPAD_ADDR + 0x028));
    gte_intpl();
    gte_stsv((SVECTOR *)(SCRPAD_ADDR + 0x028));

    return;
}

static inline int PointTestSegment_inline(HZD_SEG *wall)
{
    int z1, z2;
    int tmp;
    int height;
    int y1, y2;

    if ((wall->p1.x > *(short *)(SCRPAD_ADDR + 0x01C)) || (wall->p2.x < *(short *)(SCRPAD_ADDR + 0x014)))
    {
        return 0;
    }

    z1 = wall->p1.z;
    z2 = wall->p2.z;

    if (z2 < z1)
    {
        tmp = z1;
        z1 = z2;
        z2 = tmp;
    }

    if ((z1 > *(short *)(SCRPAD_ADDR + 0x020)) || (z2 < *(short *)(SCRPAD_ADDR + 0x018)))
    {
        return 0;
    }

    height = *(short *)(SCRPAD_ADDR + 0x016);

    y1 = wall->p1.y;
    y2 = wall->p2.y;

    if (height < y1 && height < y2)
    {
        return 0;
    }

    height = *(short *)(SCRPAD_ADDR + 0x01E);

    y1 += wall->p1.h;
    y2 += wall->p2.h;

    if (height > y1 && height > y2)
    {
        return 0;
    }

    return 1;
}

STATIC void PointTestSegment(HZD_SEG *wall, int index, int flags)
{
    int *ptr;
    int  opz;
    int  height;
    int *ptr1, *ptr2, *ptr3;

    if (!PointTestSegment_inline(wall))
    {
        return;
    }

    *(HZD_SEG *)(SCRPAD_ADDR + 0x024) = *wall;

    ptr = (int *)(SCRPAD_ADDR + 0x084);
    opz = HZD_80028930();

    if (opz >= ptr[1])
    {
        return;
    }

    if (index > *(int *)(SCRPAD_ADDR + 0x044))
    {
        HZD_80028CF8();

        height = *(short *)(SCRPAD_ADDR + 0x010) - ((HZD_SEG *)(SCRPAD_ADDR + 0x024))->p1.y;

        if (height < 0 || height > ((HZD_SEG *)(SCRPAD_ADDR + 0x024))->p1.h)
        {
            return;
        }
    }

    ptr1 = (int *)(SCRPAD_ADDR + 0x04C);
    ptr2 = (int *)(SCRPAD_ADDR + 0x068);
    ptr3 = (int *)(SCRPAD_ADDR + 0x000);

    ptr1[2] = (int)wall;
    ptr1[3] = (flags & 0x7F) | (*(int *)(ptr3 + 0x2C)) | (*(*(char **)(ptr3 + 0x2D) - index) << 8);

    if (opz < ptr2[1])
    {
        memcpy(ptr, ptr2, 28);
        memcpy(ptr2, ptr1, 28);
    }
    else if (*(int *)(SCRPAD_ADDR + 0x05C) != *(int *)(SCRPAD_ADDR + 0x078))
    {
        memcpy(ptr, ptr1, 28);
    }
    else
    {
        return;
    }

    *(int *)(SCRPAD_ADDR + 0x048) += 1;
}

static inline void sub_helper_80029098(void)
{
    if (*(int *)(SCRPAD_ADDR + 0x084) == 0)
    {
        return;
    }

    if (*(int *)(SCRPAD_ADDR + 0x068) != 0)
    {
        if (*(int *)(SCRPAD_ADDR + 0x078) != *(int *)(SCRPAD_ADDR + 0x094))
        {
            return;
        }
    }
    else
    {
        Add2D((SVECTOR *)(SCRPAD_ADDR + 0x0A0), (SVECTOR *)(SCRPAD_ADDR + 0x00C), (SVECTOR *)(SCRPAD_ADDR + 0x094));

        if (*(int *)(SCRPAD_ADDR + 0x0A0) != *(int *)(SCRPAD_ADDR + 0x07C) && *(int *)(SCRPAD_ADDR + 0x0A0) != *(int *)(SCRPAD_ADDR + 0x080))
        {
            return;
        }
    }

    *(int *)(SCRPAD_ADDR + 0x048) = 1;
}

int HZD_NearHazardCheck(HZD_HDL *hzd, SVECTOR *from, int range, int chk_flag, int seg_flag)
{
    HZD_GRP *pArea;
    int       n_unknown;
    HZD_SEG  *pWalls;
    char     *pFlags;
    int       wall_count;
    char    **ptr;
    char    **ptr2;
    int       i;
    HZD_SEG **ppWalls;
    int       idx;
    int       queue_size;

    pArea = hzd->grp;

    CopyVector(from, (HZD_VEC *)(SCRPAD_ADDR + 0x00C));
    CreateBoundingBox((HZD_VEC *)(SCRPAD_ADDR + 0x00C), range);

    *(int *)(SCRPAD_ADDR + 0x048) = 0;

    if (chk_flag & HZD_CHK_F_SEGMENT)
    {
        n_unknown = pArea->n_flat_walls;

        *(int *)(SCRPAD_ADDR + 0x088) = range * range;
        *(int *)(SCRPAD_ADDR + 0x06C) = range * range;

        do {} while (0);

        pWalls = pArea->walls;
        pFlags = pArea->wallsFlags;
        wall_count = pArea->n_walls;

        ptr = (char **)SCRPAD_ADDR;
        ptr[0x2C] = (char *)0;
        ptr[0x2D] = pFlags + wall_count * 2;

        *(int *)(SCRPAD_ADDR + 0x044) = n_unknown;

        for (i = pArea->n_walls; i > 0; i--, pWalls++, pFlags++)
        {
            if ((*pFlags & seg_flag) == 0)
            {
                PointTestSegment(pWalls, i, *pFlags);
            }
        }
    }

    if (chk_flag & HZD_CHK_D_SEGMENT)
    {
        ppWalls = hzd->dynamic_segments;
        pFlags = hzd->dynamic_flags;
        queue_size = hzd->max_dynamic_segments;
        idx = hzd->dynamic_queue_index;

        ptr2 = (char **)SCRPAD_ADDR;
        ptr2[0x2C] = (char *)0x80;
        ptr2[0x2D] = pFlags + queue_size + idx;

        *(int *)(SCRPAD_ADDR + 0x044) = 0;

        for (i = hzd->dynamic_queue_index; i > 0; i--, ppWalls++, pFlags++)
        {
            if ((*pFlags & seg_flag) == 0)
            {
                PointTestSegment(*ppWalls, i, *pFlags);
            }
        }
    }

    if (*(int *)(SCRPAD_ADDR + 0x048) > 1)
    {
        *(int *)(SCRPAD_ADDR + 0x048) = 2;
        sub_helper_80029098();
    }

    return *(int *)(SCRPAD_ADDR + 0x048);
}

void HZD_GetNearHazard(HZD_SEG **segs)
{
    segs[0] = *(HZD_SEG **)(SCRPAD_ADDR + 0x70);
    segs[1] = *(HZD_SEG **)(SCRPAD_ADDR + 0x8c);
}

void HZD_GetIsEdge(signed char *ie)
{
    ie[0] = *getScratchAddr2(char, 0x74);
    ie[1] = *getScratchAddr2(char, 0x90);
}

void HZD_GetNearVector(SVECTOR *vect_ptr)
{
    HZD_SEG *wall1;
    HZD_SEG *wall2;

    wall1 = getScratchAddr2(HZD_SEG, 0x68);
    vect_ptr[0].vx = wall1[1].p1.x;
    vect_ptr[0].vy = 0;
    vect_ptr[0].vz = wall1[1].p1.z;

    wall2 = getScratchAddr2(HZD_SEG, 0x84);
    vect_ptr[1].vx = wall2[1].p1.x;
    vect_ptr[1].vy = 0;
    vect_ptr[1].vz = wall2[1].p1.z;
}
