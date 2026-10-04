#include "loader.h"

#include <stdio.h>
#include "common.h"
#include "libgv/libgv.h"
#include "libdg/libdg.h"
#include "libfs/libfs.h"
#include "game/game.h"

typedef struct _Work
{
    GV_ACT  actor;
    void   *info;
    int     type;
    int     reading;
    int     time;
} Work;

#define EXEC_LEVEL GV_ACTOR_ASSIST

#ifdef __psyz
/* Which stage a requested name actually resolves to on this board, or NULL if
 * the request cannot be honoured at all.
 *
 * Two things need the same answer, and they run at different moments: the
 * script command that decides to change stage (game/script.c) and the loader
 * that carries the change out. Deciding twice is how "title" first got mapped
 * to a playable stage here while being rejected as missing over there, which
 * left the game with nothing to load and a blank screen.
 *
 * "title" is special because no title stage is packed and the retail select
 * menu's only entry is exactly that -- following it just reloads the menu
 * forever. Send it to the start of the game instead: s00a is the dock Snake
 * surfaces in, so a board carrying it plays MGS from the beginning, and s07a
 * is the single room the first builds shipped with, kept as the fallback.
 * Which stages are packed is decided by port/pack_stagedir.py, so ask
 * STAGE.DIR rather than assume. */
extern const char *const Mgs_PlayableStages[];  /* port/stage_union.c */

/* A stage is usable only if BOTH are true: its data is reachable, and its
 * actor table is one of the ones linked into this build. The two used to be
 * the same question because only the stages we could run were packed, but a
 * board reading the full STAGE.DIR off a card can reach all 96 -- including
 * the 28 whose tables are still raw MIPS addresses. */
static int stage_is_usable(const char *dir)
{
    int i;

    if (FS_CdGetStageFileTop((char *)dir) < 0)
    {
        return 0;
    }
    if (strcmp(dir, "init") == 0)   /* the boot stage; it spawns no actors */
    {
        return 1;
    }
    for (i = 0; Mgs_PlayableStages[i]; i++)
    {
        if (strcmp(dir, Mgs_PlayableStages[i]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

/* Where the game starts. Set MGS_START_STAGE from the build to jump straight
 * into any stage in Mgs_PlayableStages -- useful for testing somewhere with
 * enemies rather than walking there from the dock every time. The list is a
 * fallback chain: the first entry that is actually present wins, so a board
 * with a trimmed data set still boots. */
#ifndef MGS_START_STAGE
#define MGS_START_STAGE "s00a"
#endif

const char *Mgs_ResolveStage(const char *dir)
{
    static const char *const first[] = {MGS_START_STAGE, "s00a", "s07a"};
    int i;

    if (strcmp(dir, "title") == 0)
    {
        for (i = 0; i < (int)(sizeof(first) / sizeof(first[0])); i++)
        {
            if (stage_is_usable(first[i]))
            {
                return first[i];
            }
        }
        return NULL;
    }

    return stage_is_usable(dir) ? dir : NULL;
}
#endif

static void Act(Work *work)
{
    work->time++;

    if (work->type != 2)
    {
        if (work->type == 3)
        {
            DG_OffsetDispEnv(work->time & 2);
            GM_PadVibration2 = 100;
        }
    }

    if (work->reading)
    {
        if (!FS_LoadStageSync(work->info))
        {
            work->reading = FALSE;
        }
    }
    else
    {
        GV_DestroyActor(&work->actor);
    }
}

static void Die(Work *work)
{
    printf("LoadEnd\n");
    FS_LoadStageComplete(work->info);
    GM_LoadComplete = -1;
}

void *NewLoader(const char *dir)
{
    Work *work;

#if defined(DEV_EXE) || defined(__psyz)
    /* normally already done by the script command, but the game reaches here
     * from a few other paths (game over, restart) that never went through it */
    {
        const char *resolved = Mgs_ResolveStage(dir);
        if (resolved && resolved != dir)
        {
            printf("[stage] '%s' -> '%s'\n", dir, resolved);
            dir = resolved;
        }
    }
#endif

    work = GV_NewActor(EXEC_LEVEL, sizeof(Work));

    printf("LoadReq\n");
    work->info = FS_LoadStageRequest(dir);

    if (!work->info)
    {
        printf("NOT FOUND STAGE %s\n", dir);
    }

    GV_SetNamedActor(&work->actor, Act, Die, "loader.c");

    work->reading = TRUE;
    work->type = (GM_LoadRequest & 0x0f);
    GM_LoadComplete = 0;

    return (void *)work;
}
