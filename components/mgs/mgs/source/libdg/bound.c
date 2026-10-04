#include "libdg.h"
#include "common.h"
#include "game/game.h"

#ifdef __psyz
/* A bounding box that failed the on-screen test is not always really off
 * screen.
 *
 * The eight corners are projected through the GTE, and the GTE pins a screen
 * coordinate at -1024 / +1023 whenever the perspective divide overflows --
 * which it does for any vertex nearer the eye than H/2, H being the projection
 * distance DG_Clip installs (320 here, so anything within 160 units). Worse, a
 * corner BEHIND the eye comes back with its sign flipped, so an object the
 * player is standing inside can land with all eight corners on one side of the
 * screen and read as "entirely off to the right".
 *
 * The console has the same GTE and the same blind spot; it just rarely matters
 * there, because on a 30 fps machine the camera is where the designers put it.
 * Here the frame rate wanders, so an object sitting on the boundary saturates
 * on one frame and not the next: it winks in and out. That is the "algunas
 * partes tiemblan" -- geometry flickering at close range, not a texture or
 * timing problem.
 *
 * So when the box is saturated, do not trust it. Answer 1 (visible, needs
 * clipping) instead of 0 (cull) and let the rasterizer decide per pixel, which
 * it has to do for these primitives anyway. It costs the objects nearest the
 * camera -- the large ones -- but drawing something too often is a frame-rate
 * bill, while culling it wrongly is a hole in the wall.
 *
 * mgs_dbg_cull_sat  boxes kept because the projection overflowed
 * mgs_dbg_cull_off  boxes genuinely off screen, culled
 */
unsigned mgs_dbg_cull_sat, mgs_dbg_cull_off;
unsigned mgs_dbg_clip_dist;

/* Does this box actually straddle the near plane?
 *
 * The first version of this test asked whether a projected X or Y had come
 * back pinned at the GTE's -1024/+1023 limit. That is the symptom, not the
 * cause: a big object far away -- a corridor wall running past both screen
 * edges -- clamps exactly the same way while its projection is perfectly
 * trustworthy. It kept 97% of everything the screen test rejected and cost 40%
 * more rasterizing for geometry that really was out of view.
 *
 * The cause is available directly. gte_stsz3c has already left the eight
 * corners' view-space Z at SCRPAD+0x6C, and the divide overflows exactly when
 * SZ <= H/2. So ask that: only a box with a corner inside the near limit has an
 * untrustworthy projection, and only that box gets drawn anyway. */
static int mgs_box_crosses_near(void)
{
    const long *z = (const long *)(SCRPAD_ADDR + 0x6C);
    long limit = (long)(mgs_dbg_clip_dist >> 1);
    int i, behind = 0;

    for (i = 0; i < 8; i++)
    {
        if (z[i] <= limit)
        {
            behind++;
        }
    }

    /* Answering "1" here means "I cannot judge this box, draw it and let the
     * rasterizer sort it out per pixel". That is the right answer for a box
     * that STRADDLES the near plane, because its projection really is
     * meaningless. It is the wrong answer for a box entirely BEHIND it, and
     * the first version of this test could not tell the two apart -- both have
     * a corner with z <= limit, so both were drawn.
     *
     * That is not a rare case. Walking towards the dock's lift, 16090 of 21306
     * boxes came back "cannot judge", 25093 models reached a primitive queue
     * sized for a fraction of that, the queue dropped packets mid-list and the
     * GPU reader desynced on the remains -- the white screen. Most of those
     * boxes were simply behind the camera: an overhead shot puts the whole
     * floor the player just walked across out of the frustum, corner by
     * corner.
     *
     * All eight corners behind the near plane is unambiguous and needs no
     * divide: nothing in that box can appear. Cull it. Only a genuine straddle
     * keeps its licence to be drawn unjudged. */
    if (behind == 8)
    {
        return 0;
    }
    return behind > 0;
}

/* evaluates to the bound_mode to use for a box that failed the screen test */
#define MGS_CULL_RESULT(a2, a3, t0, t1)                                        \
    (mgs_box_crosses_near() ? (mgs_dbg_cull_sat++, 1)                          \
                            : (mgs_dbg_cull_off++, 0))
#else
#define MGS_CULL_RESULT(a2, a3, t0, t1) 0
#endif

STATIC void DG_WriteObjClut(DG_OBJ *obj, int idx);
STATIC void DG_WriteObjClutUV(DG_OBJ *obj, int idx);
STATIC void DG_BoundIrTexture(DG_CHANL *chanl, int idx);

static inline void copy_bounding_box_to_spad(DG_BOUND *bounds)
{
    DG_BOUND *bounding_box = (DG_BOUND *)SCRPAD_ADDR;
    bounding_box->min.vx = bounds->min.vx;
    bounding_box->min.vy = bounds->min.vy;
    bounding_box->min.vz = bounds->min.vz;

    bounding_box->max.vx = bounds->max.vx;
    bounding_box->max.vy = bounds->max.vy;
    bounding_box->max.vz = bounds->max.vz;
}

static inline void set_svec_from_bounding_box(int i, SVECTOR *svec)
{
    svec->vx = i & 1 ? ((long *)SCRPAD_ADDR)[3] : ((long *)SCRPAD_ADDR)[0];
    svec->vy = i & 2 ? ((long *)SCRPAD_ADDR)[4] : ((long *)SCRPAD_ADDR)[1];
    svec->vz = i & 4 ? ((long *)SCRPAD_ADDR)[5] : ((long *)SCRPAD_ADDR)[2];
}

void DG_BoundStart(void)
{
    /* do nothing */
}

STATIC void DG_BoundObjs(DG_OBJS *objs, int idx, unsigned int flag, int in_bound_mode)
{
    int        i, i2, i3, a2, t0, a3, t1;
    int        bound_mode;
    int        n_models;
    int        n_bounding_box_vec;
    int        ret, extra;
    long      *test;
    DG_OBJ    *obj;
    DVECTOR   *dvec;
    SVECTOR   *svec;
    DG_VECTOR *vec3_1;
    DG_VECTOR *vec3_2;
    DG_BOUND  *mdl_bounds;

    n_models = objs->n_models;
    obj = (DG_OBJ *)&objs->objs;

    for (; n_models > 0; --n_models)
    {
        bound_mode = 0;
        if (in_bound_mode)
        {
            bound_mode = 2;
            if (flag & DG_FLAG_BOUND)
            {
                gte_SetRotMatrix(&obj->screen);
                gte_SetTransMatrix(&obj->screen);

                svec = (SVECTOR *)(SCRPAD_ADDR + 0x18);
                mdl_bounds = (DG_BOUND *)&obj->model->min;
                copy_bounding_box_to_spad(mdl_bounds);
                vec3_1 = (DG_VECTOR *)(SCRPAD_ADDR + 0x30);
                vec3_2 = (DG_VECTOR *)(SCRPAD_ADDR + 0x60);
                i = 9;

                while (i > 0)
                {
                    n_bounding_box_vec = 3;
                    do
                    {
                        set_svec_from_bounding_box(i, svec);
                        ++svec;
                        --i;
                        --n_bounding_box_vec;
                    } while (n_bounding_box_vec > 0);

                    svec = (SVECTOR *)(SCRPAD_ADDR + 0x18);
                    gte_stsxy3c(vec3_1);
                    gte_stsz3c(vec3_2);

                    gte_ldv3c((SVECTOR *)(SCRPAD_ADDR + 0x18));
                    vec3_1++;
                    vec3_2++;
                    gte_rtpt_b();
                }

                gte_stsxy3c(vec3_1);
                gte_stsz3c(vec3_2);

                // probably start of another inline func
                a2 = *(short *)(SCRPAD_ADDR + 0x3C);
                t0 = *(short *)(SCRPAD_ADDR + 0x3E);
                a3 = a2;
                t1 = t0;
                dvec = (DVECTOR *)(SCRPAD_ADDR + 0x3C);

                for (i2 = 7; i2 > 0; --i2)
                {
                    dvec++;
                    // loc_800187FC:
                    if (dvec->vx < a2)
                    {
                        a2 = dvec->vx;
                    }
                    else
                    {
                        if (a3 < dvec->vx)
                            a3 = dvec->vx;
                    }
                    if (dvec->vy < t0)
                    {
                        t0 = dvec->vy;
                    }
                    else
                    {
                        if (t1 < dvec->vy)
                            t1 = dvec->vy;
                    }
                }
                // loc_80018858
                // this seems ridiculous but was the only way it matched
                if ((a2 >= 0xA1) || (a3 < -0xA0) || (t0 >= 0x71) || (t1 < -0x70))
                {
#ifdef MGS_NO_BOUND_CULL
                    /* measurement build: keep everything and let the
                     * rasterizer clip, so the screen coverage it produces is
                     * the ceiling this scene can reach. Comparing that against
                     * the normal build says whether missing picture is a
                     * culling problem at all. */
                    extra = 1;
#else
                    extra = MGS_CULL_RESULT(a2, a3, t0, t1);
#endif
                }
                else
                {
                    ret = ((a3 >= 0xA1) || (a2 < -0xA0) || (t1 >= 0x71) || (t0 < -0x70)) ? 1 : 2;
                    test = (long *)(SCRPAD_ADDR + 0x6C);
                    i3 = 8;
                    while (i3 > 0)
                    {
                        --i3;
                        if (*test)
                        {
                            extra = ret;
                            goto END;
                        }
                        test++;
                    }
#ifdef __psyz
                    {
                        /* culled although the bounding box is on screen: the
                         * eight projected Z values were all zero. Show them --
                         * if the depth FIFO is not being filled, every model
                         * disappears and characters lose their parts. */
                        static int b = 4;
                        if (b > 0)
                        {
                            long *z = (long *)(SCRPAD_ADDR + 0x6C);
                            b--;
                            printf("[zcull] bbox %d..%d,%d..%d z:", a2, a3,
                                   t0, t1);
                            for (i3 = 0; i3 < 8; i3++)
                            {
                                printf(" %ld", z[i3]);
                            }
                            printf("\n");
                        }
                    }
#endif
                    extra = 0;
                }
            END:
                ret = extra;
                bound_mode = ret;
            }
        }

        // loc_800188E4
        obj->bound_mode = bound_mode;
        if (bound_mode)
        {
            obj->free_count = 8;
            if (!obj->packs[idx])
            {
                int res = DG_MakeObjPacket(obj, idx, flag);
                if (res < 0)
                {
                    obj->bound_mode = 0;
                    if (flag & DG_FLAG_GBOUND)
                    {
                        objs->bound_mode = 0;
                        return;
                    }
                }
            }
        }
        else
        {
            if (obj->packs[idx])
            {
                --obj->free_count;
                if (obj->free_count <= 0)
                {
                    DG_FreeObjPacket(obj, idx);
                }
            }
        }
        obj++;
    }

#ifdef __psyz
    /* NOT a bug, measured: welded children always find their parent already
     * transformed. A probe counting the cases reported "culled-parent 0,
     * no-packs 0" over thousands of models -- what looked like two thirds
     * reading stale data was entirely sub-models whose parent index is -1,
     * i.e. standalone scenery objects that weld to nothing. A pass that
     * un-culled parents was written, measured at zero effect, and removed. */
#endif
}

void DG_BoundChanl(DG_CHANL *chanl, int idx)
{
    int          i, i2, i3, a2, t0, a3, t1;
    int          n_objs;
    int          bound_mode;
    DG_OBJS    **objs;
    int          local_group_id;
    DVECTOR     *dvec;
    SVECTOR     *svec;
    DG_VECTOR   *vec3_1;
    DG_VECTOR   *vec3_2;
    DG_BOUND    *mdl_bounds;
    int          n_bounding_box_vec;
    long        *test;
    unsigned int flag;

    DG_Clip(&chanl->clip_rect, chanl->clip_distance);

    objs = chanl->queue;
    n_objs = chanl->objs_index;
    local_group_id = DG_CurrentGroupID;

    for (; n_objs > 0; --n_objs)
    {
        DG_OBJS *current_objs = *objs;
        objs++;
        flag = current_objs->flag;

        bound_mode = 0;
        if (!(flag & DG_FLAG_INVISIBLE))
        {
            if (!current_objs->group_id || (current_objs->group_id & local_group_id))
            {
                bound_mode = 2;
                if (flag & DG_FLAG_GBOUND)
                {
                    gte_SetRotMatrix(&current_objs->objs->screen);
                    gte_SetTransMatrix(&current_objs->objs->screen);

                    svec = (SVECTOR *)(SCRPAD_ADDR + 0x18);
                    mdl_bounds = (DG_BOUND *)&current_objs->def->min;
                    copy_bounding_box_to_spad(mdl_bounds);
                    vec3_1 = (DG_VECTOR *)(SCRPAD_ADDR + 0x30);
                    vec3_2 = (DG_VECTOR *)(SCRPAD_ADDR + 0x60);
                    i = 9;

                    while (i > 0)
                    {
                        n_bounding_box_vec = 3;
                        do
                        {
                            set_svec_from_bounding_box(i, svec);
                            ++svec;
                            --i;
                            --n_bounding_box_vec;
                        } while (n_bounding_box_vec > 0);

                        svec = (SVECTOR *)(SCRPAD_ADDR + 0x18);
                        gte_stsxy3c(vec3_1);
                        gte_stsz3c(vec3_2);

                        gte_ldv3c((SVECTOR *)(SCRPAD_ADDR + 0x18));
                        vec3_1++;
                        vec3_2++;
                        gte_rtpt_b();
                    }

                    gte_stsxy3c(vec3_1);
                    gte_stsz3c(vec3_2);

                    // probably start of another inline func
                    a2 = *(short *)(SCRPAD_ADDR + 0x3C);
                    t0 = *(short *)(SCRPAD_ADDR + 0x3E);
                    a3 = a2;
                    t1 = t0;
                    dvec = (DVECTOR *)(SCRPAD_ADDR + 0x3C);

                    for (i2 = 7; i2 > 0; --i2)
                    {
                        dvec++;
                        if (dvec->vx < a2)
                        {
                            a2 = dvec->vx;
                        }
                        else
                        {
                            if (a3 < dvec->vx)
                                a3 = dvec->vx;
                        }
                        if (dvec->vy < t0)
                        {
                            t0 = dvec->vy;
                        }
                        else
                        {
                            if (t1 < dvec->vy)
                                t1 = dvec->vy;
                        }
                    }

                    if ((a2 >= 0xA1) || (a3 < -0xA0) || (t0 >= 0x71) || (t1 < -0x70))
                    {
#ifdef MGS_NO_BOUND_CULL
                        /* experiment: keep everything, clipped by the GPU */
                        bound_mode = 1;
#else
                        bound_mode = MGS_CULL_RESULT(a2, a3, t0, t1);
#endif
                    }
                    else
                    {
                        bound_mode = ((a3 >= 0xA1) || (a2 < -0xA0) || (t1 >= 0x71) || (t0 < -0x70)) ? 1 : 2;
                        test = (long *)(SCRPAD_ADDR + 0x6C);
                        i3 = 8;
                        while (i3 > 0)
                        {
                            --i3;
                            if (*test)
                            {
                                goto END;
                            }
                            test++;
                        }
                        bound_mode = 0;
                    }
                END:
                }
            }
        }
        // loc_80018CE0:
        current_objs->bound_mode = bound_mode;
        DG_BoundObjs(current_objs, idx, flag, bound_mode);
    }

    DG_BoundIrTexture(chanl, idx);
}

void DG_BoundEnd(void)
{
    /* do nothing */
}

// Possibly a different file.

STATIC DG_TEX DG_UnknownTexture = {0};

/* Replace the CLUT for this model with a plain white one for thermal goggles */
STATIC void DG_WriteObjClut(DG_OBJ *obj, int idx)
{
    int       n_packs;
    POLY_GT4 *pPack = obj->packs[idx];
    short     val = 0x3FFF;
    if (pPack && pPack->clut != val)
    {
        while (obj)
        {
            n_packs = obj->n_packs;
            while (n_packs > 0)
            {
                pPack->clut = val;

                ++pPack;
                --n_packs;
            }

            obj = obj->extend;
        }
    }
}

/* Restore the CLUT for this model */
STATIC void DG_WriteObjClutUV(DG_OBJ *obj, int idx)
{
    unsigned short id;
    POLY_GT4      *pack;
    int            n_packs;
    short         *tex_ids;
    DG_TEX        *texture;
    unsigned short current_id;

    pack = obj->packs[idx];

    if (pack && pack->clut == 0x3FFF)
    {
        texture = &DG_UnknownTexture;
        id = 0;
        while (obj)
        {
            tex_ids = obj->model->materials;
            for (n_packs = obj->n_packs; n_packs > 0; --n_packs)
            {
                current_id = *tex_ids;
                tex_ids++;
                if ((current_id & 0xFFFF) != id)
                {
                    id = current_id;
                    texture = DG_GetTexture(id);
                }
                pack->clut = texture->clut;
                pack++;
            }
            obj = obj->extend;
        }
    }
}

// there must be a way to match this without the repetition
STATIC void DG_BoundIrTexture(DG_CHANL *chanl, int idx)
{
    DG_OBJS **queue;
    int       n_objects;
    DG_OBJS  *objs;
    DG_OBJ   *obj;
    int       n_models;

    queue = chanl->queue;
    if (GM_GameStatus & STATE_THERMG)
    {
        for (n_objects = chanl->objs_index; n_objects > 0; n_objects--)
        {
            objs = *queue++;

            if (objs->flag & DG_FLAG_IRTEXTURE && objs->bound_mode != 0)
            {
                obj = objs->objs;

                for (n_models = objs->n_models; n_models > 0; n_models--)
                {
                    if (obj->bound_mode != 0)
                    {
                        DG_WriteObjClut(obj, idx);
                    }

                    obj++;
                }
            }
        }
    }
    else
    {
        for (n_objects = chanl->objs_index; n_objects > 0; n_objects--)
        {
            objs = *queue++;

            if (objs->flag & DG_FLAG_IRTEXTURE && objs->bound_mode != 0)
            {
                obj = objs->objs;

                for (n_models = objs->n_models; n_models > 0; n_models--)
                {
                    DG_WriteObjClutUV(obj, idx);
                    obj++;
                }
            }
        }
    }
}
