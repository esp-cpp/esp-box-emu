#include <stdio.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include "common.h"
#include "libdg/libdg.h"
#include "libgv/libgv.h"
#include "mts/mts.h"
#include "game/game.h"

extern GV_PAD *GM_CurrentPadData;

/*---------------------------------------------------------------------------*/

typedef struct {
    GV_ACT actor;
} Work;

extern Work DG_WorkFirst;
extern Work DG_WorkLast;
extern int dword_800B3790;

int DG_FrameRate = 2;
int DG_HikituriFlag = 0;        // 引きつり = twitching
int DG_HikituriFlagOld = 0;     //   〃          〃

STATIC int DG_TickCount = -1;

/*---------------------------------------------------------------------------*/

#ifdef __psyz
unsigned mgs_dbg_vsynccb, mgs_dbg_drawotag;
#endif

int DG_VSyncCallbackFunc(void)
{
#ifdef __psyz
    mgs_dbg_vsynccb++;
#endif
    if (DrawSync(1) > 0)
    {
        dword_800B3790++;
        if (dword_800B3790 < 30)
        {
            return 0;
        }
        printf("@");
        ResetGraph(1);
        dword_800B3790 = 0;
    }
    return 1;
}

void DG_ActFirst(Work *work)
{
    int ticks;

    dword_800B3790 = 0;

    DG_HikituriFlagOld = DG_HikituriFlag;

#ifdef __psyz
    /* Konami's frame-drop-to-catch-up ("hikituri" = stutter) mechanism, off.
     *
     * When a frame overruns its budget the flag is raised, and then TWO
     * different actors consult it: DG_SwapFrame skips DG_DrawOTag, and
     * libgv/gvd.c's Act() skips the GV_Clock buffer flip. On the console
     * those two decisions are always taken from the same value, so a dropped
     * frame just repeats the previous picture. Here the actors run at
     * different points of the list and can read the flag from different
     * frames -- flip without draw, and the panel shows a buffer nobody drew:
     * a black field between good ones, which is the flicker on the LCD.
     *
     * The mechanism exists to hold a 33 ms real-time budget this board
     * cannot promise anyway. Pinning both flags low keeps every frame drawn
     * and every flip paired with a draw: a heavy scene now runs slower
     * instead of strobing. */
    DG_HikituriFlagOld = 0;
    DG_HikituriFlag = 0;
#endif

    if (GM_GameStatus & STATE_NOSLOW)
    {
        if (DG_TickCount == -1)
        {
            DG_TickCount = mts_get_tick_count();
            DG_HikituriFlag = 0;
        }

        if (!DG_HikituriFlag)
        {
            mts_wait_vbl(DG_FrameRate);
        }

        ticks = mts_get_tick_count();
#ifdef __psyz
        /* see the note above: never raise it */
        (void)ticks;
        DG_HikituriFlag = 0;
#else
        if (DG_TickCount + 2 < ticks)
        {
            DG_HikituriFlag = 1;
        }
        else
        {
            DG_HikituriFlag = 0;
        }
#endif

        DG_TickCount += 2;
    }
    else
    {
        mts_wait_vbl(DG_FrameRate);
        DG_TickCount = -1;
        DG_HikituriFlag = 0;
    }

    DG_SwapFrame();

    GV_UpdatePadSystem();
#ifdef __psyz
    /* The keyboard-over-serial pad ages its held buttons here, not on the
     * vblank tick: this is the one point that means "a frame has read the
     * pad", and the frame rate is not tied to the vblank rate. */
    {
        void Mgs_PadConsumed(void);
        Mgs_PadConsumed();
    }
#endif
    GM_CurrentPadData = GV_PadData;

    if ((GM_PlayerStatus & PLAYER_SECOND_AVAILABLE) != 0)
    {
        if (GV_PadData[1].status | GV_PadData[1].release)
        {
            GM_CurrentPadData = &GV_PadData[1];
        }
    }
}

void DG_ActLast(Work *work)
{
    DG_RenderFrame();
}

void DG_ResetPipeline(void)
{
    DG_InitLightSystem();
    DG_RenderPipeline_Init();

    DG_SetCurrentGroup(0);

    DG_ReloadPalette();
    DG_ResetPaletteEffect();
    DG_SetRGB(0, 0, 0);

    printf("Object Queue %d\n", DG_Chanl(0)->objs_index);
    printf("Primitive Queue %d\n", DG_Chanl(0)->queue_size - DG_Chanl(0)->prim_index);

    DG_Chanl(0)->objs_index = 0;
    DG_Chanl(0)->prim_index = DG_Chanl(0)->queue_size;
}

void DG_ResetTextureCache(void)
{
    DG_InitTextureSystem();
    DG_LoadResidentTextureCache();
}

void DG_StartDaemon(void)
{
    mts_set_vsync_task();
    mts_set_vsync_callback_func(DG_VSyncCallbackFunc);

    DG_InitDispEnv(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 320);
    DG_InitChanlSystem(SCREEN_WIDTH);
    DG_ClearResidentTexture();
    DG_ResetPipeline();

    GV_SetLoader('p', DG_LoadInitPcx);      // *.pcx format
    GV_SetLoader('k', DG_LoadInitKmd);      // *.kmd format
    GV_SetLoader('l', DG_LoadInitLit);      // *.lit format
    GV_SetLoader('n', DG_LoadInitNar);      // *.nar format
    GV_SetLoader('o', DG_LoadInitOar);      // *.oar format
    GV_SetLoader('z', DG_LoadInitKmdar);    // *.zmd format
    GV_SetLoader('i', DG_LoadInitImg);      // *.img format
    GV_SetLoader('s', DG_LoadInitSgt);      // *.sgt format

    // Wait for vsync, swap frame, fetch input
    GV_InitActor(GV_ACTOR_DAEMON, &DG_WorkFirst, NULL);
    GV_SetNamedActor(&DG_WorkFirst, DG_ActFirst, NULL, "dgd.c");

    // Render new frame
    GV_InitActor(GV_ACTOR_DAEMON2, &DG_WorkLast, NULL);
    GV_SetNamedActor(&DG_WorkLast, DG_ActLast, NULL, "dgd.c");
}
