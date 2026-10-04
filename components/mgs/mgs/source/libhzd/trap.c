#include "libhzd.h"

#include "common.h"
#include "strcode.h"
#include "game/game.h"

typedef	struct
{
    HZD_VEC from;
    HZD_SEG current;
    u_int   n_inside;
    u_short inside[6];
} ScrPad;

#define SCRPAD      ((ScrPad *)SCRPAD_ADDR)

#define	FROM		(&(SCRPAD->from))
#define	CURRENT		(&(SCRPAD->current))
#define	N_INSIDE    (&(SCRPAD->n_inside))
#define	INSIDE      (SCRPAD->inside)

/*----------------------------------------------------------------*/

static inline void CopyInsideList(HZD_EVT *ev)
{
    u_short *from, *to;
    int      i;

    from = ev->inside;
    to = INSIDE;
    *N_INSIDE = ev->n_inside;
    for (i = ev->n_inside; i > 0; i--)
    {
        *to++ = *from++;
    }
}

// TODO: remove in argument
static inline int DeleteInsideList(u_short *in, int name)
{
    u_short *inside;
    int      i;

    inside = INSIDE;
    for (i = *N_INSIDE; i > 0; i--)
    {
        if (*inside++ == name)
        {
            (*N_INSIDE)--;
            inside[-1] = (in + 0x0E)[*N_INSIDE];
            return 1;
        }
    }
    return 0;
}

static inline int AppendInsideList(u_short *inside, int n_inside, int name)
{
    int i;

    for (i = n_inside; i > 0; i--)
    {
        if (*inside++ == name)
        {
            return n_inside;
        }
    }

    *inside = name;
    return n_inside + 1;
}

/*----------------------------------------------------------------*/

#ifdef __psyz
/* Counted at the hit site further down and printed by the periodic scan
 * report; defined in port/psyz_port.c so both blocks see the same one. */
extern int mgs_dbg_trap_hits;
#endif

static inline int InsideTrap(void)
{
    int d;

    d = FROM->x;
    if (d < CURRENT->p1.x || d >= CURRENT->p2.x) return 0;
    d = FROM->z;
    if (d < CURRENT->p1.z || d >= CURRENT->p2.z) return 0;
    d = FROM->y;
    if (d < CURRENT->p1.y || d >= CURRENT->p2.y) return 0;
    return 1;
}

static void ExecEnterEvent(HZD_HDL *hzd, HZD_EVT *ev)
{
    HZD_GRP *grp;
    HZD_TRP *trp;
    void    *scr;
    int      i, n_inside;
    int      name;

    grp = hzd->grp;
    trp = (HZD_TRP *)grp->triggers;

    ev->type = HASH_ENTER;

#ifdef __psyz
    {
        /* Triggers are how MGS grows the world: crossing one runs a script
         * that does "add map N". Only map 1 was ever added on this board, so
         * the rest of the dock has no geometry and no collision -- black to
         * the sides, and nowhere to walk to. Either no trigger is ever being
         * entered, or the list being scanned is empty. Say which, once every
         * few seconds, and report the first hit so a working one is
         * distinguishable from none at all. */
        static int beat;
        if ((++beat % 600) == 0)
        {
            printf("[trap] scanning %d triggers (%d cameras), %d entered so far\n",
                   grp->n_triggers - hzd->n_cameras, hzd->n_cameras,
                   mgs_dbg_trap_hits);
        }
    }
#endif

    n_inside = 0;
    for (i = grp->n_triggers - hzd->n_cameras; i > 0; i--, trp++)
    {
        scr = SCRPAD;
        *CURRENT = *(HZD_SEG *)trp;

        if (!InsideTrap())
        {
            continue;
        }
#ifdef __psyz
        {
            /* Report each trigger the FIRST time it is crossed, rather than
             * the first twelve crossings. The ones around the spawn point fire
             * at once and used to eat the whole budget, so walking somewhere
             * interesting -- the elevator -- produced nothing at all. With a
             * seen-list the log stays quiet until new ground is covered, and
             * the name that appears when the player reaches something that
             * does not respond IS the answer to why it does not. */
            static short seen[64];
            static int   n_seen;
            int          k, fresh = 1;

            mgs_dbg_trap_hits++;
            for (k = 0; k < n_seen; k++)
            {
                if (seen[k] == (short)trp->name_id) { fresh = 0; break; }
            }
            if (fresh)
            {
                if (n_seen < (int)(sizeof(seen) / sizeof(seen[0])))
                {
                    seen[n_seen++] = (short)trp->name_id;
                }
                printf("[trap] NEW trigger '%.12s' id %d at %d,%d,%d\n",
                       trp->name, trp->name_id, ev->mov.vx, ev->mov.vy,
                       ev->mov.vz);
            }
        }
#endif

        name = trp->name_id;
        ev->object = name;

        if (!DeleteInsideList(scr, name))
        {
            HZD_ExecEvent(hzd, ev, 1);
        }
        else
        {
            HZD_ExecEvent(hzd, ev, 2);
        }

        n_inside = AppendInsideList(ev->inside, n_inside, name);
    }

    ev->n_inside = n_inside;
}

static void ExecLeaveEvent(HZD_HDL *hzd, HZD_EVT *ev)
{
    u_short *inside;
    int      i;

    ev->type = HASH_LEAVE;

    inside = INSIDE;
    for (i = *N_INSIDE; i > 0; i--)
    {
        ev->object = *inside++;
        HZD_ExecEvent(hzd, ev, 0);
    }
}

void HZD_EnterTrap(HZD_HDL *hzd, HZD_EVT *ev)
{
    SVECTOR *mov;
    short    tmp;
    u_short *from, *to;
    int      i;

    mov = &ev->mov;

    *(short *)(SCRPAD_ADDR + 0x000) = mov->vx;
    do {} while (0);

    *(short *)(SCRPAD_ADDR + 0x004) = mov->vy;
    from = ev->inside;
    do {} while (0);

    tmp = mov->vz;
    to = INSIDE;

    do {} while (0);

    *(short *)(SCRPAD_ADDR + 0x002) = tmp;
    *N_INSIDE = ev->n_inside;

    for (i = ev->n_inside; i > 0; i--)
    {
        *to++ = *from++;
    }

    ExecEnterEvent(hzd, ev);
    ExecLeaveEvent(hzd, ev);
}

// TODO: move

#define HZD_COPY_ELEM(dst, src) \
do {                            \
    *(dst) = (src);             \
} while (0)

#define HZD_COPY_VEC(dst, src)                        \
do {                                                  \
    HZD_COPY_ELEM(&((HZD_VEC *)(dst))->x, (src)->vx); \
    HZD_COPY_ELEM(&((HZD_VEC *)(dst))->y, (src)->vy); \
    HZD_COPY_ELEM(&((HZD_VEC *)(dst))->z, (src)->vz); \
} while (0)

HZD_TRP *HZD_CheckBehindTrap(HZD_HDL *hzd, SVECTOR *pos)
{
    int      i;
    HZD_TRP *trap;

    HZD_COPY_VEC(FROM, pos);

    for (i = hzd->n_cameras, trap = hzd->traps; i > 0; i--, trap++)
    {
        *CURRENT = *(HZD_SEG *)trap;

        if (InsideTrap())
        {
            return trap;
        }
    }

    return NULL;
}
