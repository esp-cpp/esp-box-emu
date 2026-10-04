#include "libdg.h"
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "common.h"

typedef	struct
{
    MATRIX eye;
    MATRIX world;
    MATRIX temp[ 24 ];
    MATRIX root;
    MATRIX world2;
} ScrPad;

#define	SCRPAD  ((ScrPad *)SCRPAD_ADDR)

#define	EYE     (&(SCRPAD->eye))
#define	WORLD   (&(SCRPAD->world))
#define	TEMP    (SCRPAD->temp)
#define	ROOT    (&(SCRPAD->root))
#define	WORLD2  (&(SCRPAD->world2))

STATIC void DG_ScreenModelsSingle( DG_OBJS *objs, int n_models )
{
    DG_OBJ *obj;

    // view x world -> screen
    gte_CompMatrix(EYE, WORLD, TEMP);

    obj = objs->objs;
    for (; n_models > 0; n_models--)
    {
        obj->world = *WORLD;
        obj->screen = *TEMP;
        obj++;
    }
}

void DG_ScreenModels( DG_OBJS *objs, int n_models )
{
    MATRIX *screen;
    DG_OBJ *obj;

    gte_SetRotMatrix(EYE);
    gte_SetTransMatrix(EYE);

    screen = TEMP;

    obj = objs->objs;
    for (; n_models > 0; n_models--)
    {
        DG_CompMatrix(screen, &obj->screen);
        obj++;
        screen++;
    }
}

#ifdef VR_EXE
STATIC void DG_ScreenModelsEnvmap( DG_OBJS *objs, int n_models )
{
    MATRIX *screen;
    DG_OBJ *obj;

    gte_SetRotMatrix(EYE);
    gte_SetTransMatrix(EYE);

    screen = TEMP;

    obj = objs->objs;
    for (; n_models > 0; n_models--)
    {
        *screen = obj->world;

        DG_CompMatrix(screen, &obj->screen);
        obj++;
        screen++;
    }
}
#endif

STATIC void DG_ApplyMovs( DG_OBJS *objs, int n_models )
{
    SVECTOR *movs;
    MATRIX  *world;
    MATRIX  *out;
    DG_OBJ  *obj;
    MATRIX  *parent;

    movs = objs->movs;

    world = WORLD;
    gte_SetRotMatrix(world);

    out = TEMP;

    obj = objs->objs;
    for (; n_models > 0; n_models--)
    {
        parent = TEMP + obj->model->parent;

        gte_SetTransMatrix(parent);
        gte_ldv0(movs);
        gte_rt();
        gte_ReadRotMatrix(out);
        gte_stlvnl(out->t);

        obj->world = *out;

        movs++;
        obj++;
        out++;
    }
}

STATIC void DG_ApplyRots( DG_OBJS *objs, int n_models )
{
    MATRIX  *out;
    DG_OBJ  *obj;
    MATRIX  *world;
    SVECTOR *rots;
    SVECTOR *adjust;
    SVECTOR *waist_rot;
    DG_MDL  *model;
    MATRIX  *root;
    int      i;
    MATRIX  *parent;
    MATRIX  *modelm;

    out = TEMP;
    obj = objs->objs;
    world = WORLD2;
    rots = objs->rots;
    adjust = objs->adjust;
    waist_rot = objs->waist_rot;
    model = obj->model;
    root = ROOT;

#ifdef MGS_TURNTABLE
    /* Turntable: present every rig upright and face-on to whatever camera the
     * stage happens to have picked, spinning about the screen's vertical.
     *
     * A fixed rotation is not enough. s11e watches its room from overhead, so a
     * character turning about its own Y axis only ever shows the top of its
     * head -- which is exactly the view that hides the joints worth judging.
     * Deriving the pose from the view matrix makes the shot independent of the
     * camera: EYE is orthonormal, so its transpose is its inverse, and
     *
     *     WORLD.R = EYE^T . Ry(angle)
     *
     * cancels the camera out and leaves precisely the requested spin in view
     * space. WORLD is what the pelvis hangs off and what root is composed
     * against, so the whole hierarchy follows; MulMatrix0 touches only the 3x3,
     * leaving the translation, so the character stays where the game put it and
     * the scenery around it is untouched.
     *
     * Only rigs: n_models is 16 for a person and small for scenery. */
    if ( n_models >= 12 )
    {
        extern int mgs_dbg_spin;
        int        s = rsin( mgs_dbg_spin ), c = rcos( mgs_dbg_spin );
        MATRIX     inv, ry;
        int        a, b;

        for ( a = 0; a < 3; a++ )
        {
            for ( b = 0; b < 3; b++ )
            {
                inv.m[a][b] = EYE->m[b][a];
            }
        }

        ry.m[0][0] =  c; ry.m[0][1] =    0; ry.m[0][2] = s;
        ry.m[1][0] =  0; ry.m[1][1] = 4096; ry.m[1][2] = 0;
        ry.m[2][0] = -s; ry.m[2][1] =    0; ry.m[2][2] = c;

        MulMatrix0( &inv, &ry, WORLD );
    }
#endif

#ifdef __psyz
    {
        /* MEASUREMENT, not a fix.
         *
         * Every number we have about this skeleton -- the bind pose, the joint
         * map, the hinge signs -- was computed on a PC from the retail disc.
         * None of it has ever been compared against what this board actually
         * computes, and three visual guesses have now failed. So: freeze the
         * pose (all angles zero, no adjust, no waist twist, world = identity)
         * and print the sixteen resulting joint positions. In that state the
         * answer is arithmetic -- each joint sits at the sum of the pos
         * offsets down its chain -- so the board's output can be checked
         * against exact integers with no interpretation and no human looking
         * at the panel. */
        extern int mgs_dbg_bindpose;
        static SVECTOR mgs_zero_rots[32];
        if ( mgs_dbg_bindpose )
        {
            rots = mgs_zero_rots;
            adjust = 0;
            waist_rot = 0;
            *WORLD = DG_ZeroMatrix;
        }
    }
#endif

    if ( waist_rot )
    {
        RotMatrixZYX_gte( waist_rot, root );
    }
    else
    {
        RotMatrixZYX_gte( rots, root );
    }

    root->t[0] = model->pos.vx;
    root->t[1] = model->pos.vy;
    root->t[2] = model->pos.vz;

    if ( !adjust )
    {
        gte_CompMatrix( WORLD, root, root );
    }
    else
    {
        *world = *WORLD;
        *WORLD = DG_ZeroMatrix;
    }

    for ( i = n_models; i > 0; i-- )
    {
        model = obj->model;
        parent = TEMP + model->parent;

        RotMatrixZYX_gte( rots, out );

        out->t[0] = model->pos.vx;
        out->t[1] = model->pos.vy;
        out->t[2] = model->pos.vz;

        if ( i == ( n_models - 1 ) )
        {
            modelm = root;
        }
        else
        {
            modelm = parent;
        }

        gte_CompMatrix( modelm, out, out );

        if ( !adjust )
        {
            obj->world = *out;
        }
        else
        {
            if ( adjust->vz ) RotMatrixZ( adjust->vz, out );
            if ( adjust->vx ) RotMatrixX( adjust->vx, out );
            if ( adjust->vy ) RotMatrixY( adjust->vy, out );
            adjust++;
        }

        obj++;
        out++;
        rots++;
    }

    if ( adjust )
    {
        out = TEMP;
        obj = objs->objs;

        gte_SetRotMatrix( world );
        gte_SetTransMatrix( world );

        for ( i = n_models; i > 0; i-- )
        {
            DG_CompMatrix( out, out );
            obj->world = *out;
            obj++;
            out++;
        }
    }

#ifdef __psyz
    {
        /* Dump the assembled skeleton. Three checks per joint, each of which
         * fails loudly and names the culprit:
         *   det  must be 4096 -- a value of -4096 is a REFLECTION, which by
         *        itself explains a knee bending backwards and feet pointing
         *        opposite ways
         *   d2   the squared distance from this joint to its parent, which in
         *        the bind pose must equal the squared length of its own pos
         *        offset; anything else means composition is not rigid
         *   t    the absolute position, to compare against the disc */
        extern int mgs_dbg_skel;

        /* Every rig the engine assembles gets audited, not just the one that
         * happened to be on screen when a burst fired. Two properties have to
         * hold for ANY character in ANY pose, so they can be checked without
         * knowing which character this is or what it is doing:
         *
         *   det == 4096   the matrix is a rotation. A negative determinant is a
         *                 reflection, which alone explains a knee that bends
         *                 backwards and feet pointing opposite ways.
         *   d2 == want    a joint sits exactly its own bone length from its
         *                 parent. Rigid composition cannot stretch a limb.
         *
         * Both drift a little from the truncating fixed-point multiplies, hence
         * the tolerances; anything outside them is a real defect. This is what
         * answers "are the soldiers assembled correctly too" -- they use the
         * same 16-bone rig and the same code, so the moment one walks on screen
         * it is audited and any complaint names it. Silence means clean. */
        {
            static unsigned joints, bad, rigs, scaled, beat;
            int ref_det = 0, ref_ratio = -1, rig_scaled = 0;
            int k, longest = -1, longest_want = 0;

            rigs++;

            /* Pick the reference bone before comparing anything: the LONGEST
             * one. The ratio is d2/want, and both are squares of quantities the
             * fixed-point pipeline rounds by about a unit, so a short bone
             * carries enormous relative noise -- the chest offset is 5 units
             * and its ratio swings by a third from rounding alone. Referencing
             * the thigh instead puts the noise floor near 0.5%, which is what
             * makes a 12% tolerance meaningful rather than decorative. */
            for ( k = 0; k < n_models; k++ )
            {
                SVECTOR *q = &objs->objs[ k ].model->pos;
                int      w = q->vx * q->vx + q->vy * q->vy + q->vz * q->vz;
                if ( w > longest_want )
                {
                    longest_want = w;
                    longest = k;
                }
            }
            if ( longest >= 0 )
            {
                DG_OBJ *o  = &objs->objs[ longest ];
                int     p  = o->model->parent;
                MATRIX *m  = &o->world;
                MATRIX *pm = ( p >= 0 ) ? &objs->objs[ p ].world : m;
                int dx = m->t[0] - pm->t[0];
                int dy = m->t[1] - pm->t[1];
                int dz = m->t[2] - pm->t[2];
                ref_ratio = (int)( ( (long long)( dx * dx + dy * dy + dz * dz )
                                     << 8 ) / longest_want );
            }
            for ( k = 0; k < n_models; k++ )
            {
                DG_OBJ *o  = &objs->objs[ k ];
                MATRIX *m  = &o->world;
                int     p  = o->model->parent;
                MATRIX *pm = ( p >= 0 ) ? &objs->objs[ p ].world : m;
                int dx = m->t[0] - pm->t[0];
                int dy = m->t[1] - pm->t[1];
                int dz = m->t[2] - pm->t[2];
                int d2 = dx * dx + dy * dy + dz * dz;
                int want = o->model->pos.vx * o->model->pos.vx +
                           o->model->pos.vy * o->model->pos.vy +
                           o->model->pos.vz * o->model->pos.vz;
                int c0 = ( m->m[1][1] * m->m[2][2] - m->m[1][2] * m->m[2][1] ) >> 12;
                int c1 = ( m->m[1][0] * m->m[2][2] - m->m[1][2] * m->m[2][0] ) >> 12;
                int c2 = ( m->m[1][0] * m->m[2][1] - m->m[1][1] * m->m[2][0] ) >> 12;
                int det = ( m->m[0][0] * c0 - m->m[0][1] * c1 + m->m[0][2] * c2 ) >> 12;
                const char *why = 0;

                joints++;

                if ( k == 0 )
                {
                    ref_det = det;
                    rig_scaled = ( det < 3891 || det > 4301 );
                }

                /* A rig may legitimately be scaled -- the game animates that,
                 * and a uniform scale is not a defect, it is an effect. So the
                 * test is not "det is 4096" but "every bone agrees with every
                 * other bone", which holds under any similarity and fails the
                 * moment composition stops being rigid:
                 *
                 *   det < 0        a REFLECTION: the classic knee that bends
                 *                  backwards and feet pointing opposite ways
                 *   det != ref_det one bone scaled differently from the rest,
                 *                  i.e. the matrix is no longer a similarity
                 *   d2/want        the same across the whole rig. Comparing the
                 *                  ratio rather than the value is what makes
                 *                  this scale-invariant, so no cube root and no
                 *                  assumption about what the game is doing.
                 */
                /* Deliberately relative, sign included. An absolute "det must be
                 * positive" test flags every shadow and every water reflection
                 * in the dock -- those are drawn by mirroring and flattening
                 * the character on purpose, so the whole rig comes out negative
                 * and hugely anisotropic, and it is correct. What can never
                 * happen is ONE bone disagreeing with the other fifteen. */
                if ( ( det ^ ref_det ) < 0 )
                {
                    why = "mirrored relative to its own rig";
                }
                else if ( det - ref_det > ( ( ref_det < 0 ? -ref_det : ref_det ) >> 4 ) ||
                          ref_det - det > ( ( ref_det < 0 ? -ref_det : ref_det ) >> 4 ) )
                {
                    why = "scale differs from rig";
                }
                else if ( want > 64 && ref_ratio > 0 )
                {
                    /* 12% on the ratio of squares is 6% on the length -- well
                     * clear of the rounding floor, and far under the breakage
                     * that was visible: a limb that had stopped following its
                     * parent was out by whole multiples, not percent. */
                    int ratio = (int)( ( (long long)d2 << 8 ) / want );
                    int tol   = ( ref_ratio >> 3 ) + 8;
                    if ( ratio - ref_ratio > tol || ref_ratio - ratio > tol )
                    {
                        why = "bone length inconsistent";
                    }
                }

                if ( why )
                {
                    bad++;
                    if ( bad <= 24 )
                    {
                        printf( "[rig] BAD n=%d joint %d parent %d det=%d "
                                "(rig %d) d2=%d want=%d model=%p -- %s\n",
                                n_models, k, p, det, ref_det, d2, want,
                                (void *)o->model, why );
                    }
                }
            }

            scaled += rig_scaled;

            if ( ( beat++ % 900u ) == 0u )
            {
                printf( "[rig] %u rigs (%u scaled by the game), %u joints, "
                        "%u bad\n",
                        rigs, scaled, joints, bad );
            }
        }

        if ( mgs_dbg_skel > 0 )
        {
            int k;
            mgs_dbg_skel--;
            printf( "SKEL n=%d adjust=%d waist=%d\n", n_models,
                    objs->adjust ? 1 : 0, objs->waist_rot ? 1 : 0 );
            for ( k = 0; k < n_models; k++ )
            {
                DG_OBJ *o = &objs->objs[ k ];
                MATRIX *m = &o->world;
                int     p = o->model->parent;
                MATRIX *pm = ( p >= 0 ) ? &objs->objs[ p ].world : m;
                int dx = m->t[0] - pm->t[0];
                int dy = m->t[1] - pm->t[1];
                int dz = m->t[2] - pm->t[2];
                int c0 = ( m->m[1][1] * m->m[2][2] - m->m[1][2] * m->m[2][1] ) >> 12;
                int c1 = ( m->m[1][0] * m->m[2][2] - m->m[1][2] * m->m[2][0] ) >> 12;
                int c2 = ( m->m[1][0] * m->m[2][1] - m->m[1][1] * m->m[2][0] ) >> 12;
                int det = ( m->m[0][0] * c0 - m->m[0][1] * c1 + m->m[0][2] * c2 ) >> 12;

                printf( " %2d p%3d rot(%4d %4d %4d) t(%7d %7d %7d) d2=%d want=%d det=%d\n",
                        k, p, objs->rots[k].vx, objs->rots[k].vy, objs->rots[k].vz,
                        m->t[0], m->t[1], m->t[2], dx * dx + dy * dy + dz * dz,
                        o->model->pos.vx * o->model->pos.vx +
                        o->model->pos.vy * o->model->pos.vy +
                        o->model->pos.vz * o->model->pos.vz, det );
            }
        }
    }
#endif
}

STATIC void DG_ScreenObjs( DG_OBJS *objs )
{
    int n_models;

    n_models = objs->n_models;

    if (objs->root)
    {
        objs->world = *objs->root;
    }

    *WORLD = objs->world;

    if (objs->flag & DG_FLAG_ONEPIECE)
    {
        DG_ScreenModelsSingle(objs, n_models);
    }
#ifdef VR_EXE
    else if (objs->flag & DG_FLAG_ENVMAP)
    {
        DG_ScreenModelsEnvmap(objs, n_models);
    }
#endif
    else
    {
        if (objs->rots)
        {
            DG_ApplyRots(objs, n_models);
        }
        else if (objs->movs)
        {
            DG_ApplyMovs(objs, n_models);
        }

        DG_ScreenModels(objs, n_models);
    }
}

void DG_ScreenChanl( DG_CHANL *chanl, int idx )
{
    DG_OBJS **queue;
    int       i;

    queue = chanl->queue;

    *EYE = chanl->eye_inv;
    DG_AdjustOverscan(EYE);

    for (i = chanl->objs_index; i > 0; i--)
    {
        DG_ScreenObjs(*queue++);
    }
}
