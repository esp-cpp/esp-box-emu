#define __LIBDG_DISPLAY_C__

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include "libdg.h"
#include "common.h"
#include "libgv/libgv.h"
#include "menu/menuman.h"
#ifdef __psyz
#include "game/camera.h"   /* GM_Camera, for the runaway-view probe */
#endif

/*** data ***/
STATIC VECTOR SECTION(".data") DG_UpVector = {0, -4096, 0, 0};

/*** $gp ***/
int DG_UnDrawFrameCount = 0;
STATIC int DG_CurrentBuffer = -1;

STATIC int SECTION(".sbss") gClipHeights_800AB960[2];
int   SECTION(".sbss") DG_CurrentGroupID;
short SECTION(".sbss") DG_ClipMin[2];
short SECTION(".sbss") DG_ClipMax[2];

/*** bss ***/
extern DISPENV g_dispenv;
extern VECTOR  DG_RightVector;

void DG_InitDispEnv(int x, short y, short w, short h, int clipH)
{
    DISPENV *dispenv = &g_dispenv;
    RECT    *disp;
    RECT    *screen;

    disp = &dispenv->disp;
    screen = &dispenv->screen;

    setRECT(disp, x, y, w, h);
    setRECT(screen, 0, 0, 256, SCREEN_HEIGHT);

    dispenv->isinter = 0;
    dispenv->isrgb24 = 0;

    // For some reason lets overwrite what we already setup
    dispenv->screen.y = 8;
    dispenv->screen.h = FRAME_HEIGHT;

    gClipHeights_800AB960[0] = x;
    gClipHeights_800AB960[1] = x + clipH;
}

void DG_ChangeReso(int flag)
{
#if 0
    DRAWENV drawenv;

    if ((flag & 1) == 0) {
        g_dispenv.disp.w = 320;
        g_dispenv.screen.x = 0;
        g_dispenv.screen.w = 255;
    } else {
        g_dispenv.disp.w = 384;
        g_dispenv.screen.x = 26;
        g_dispenv.screen.w = 212;
    }

    if ((flag & 2) == 0) {
        g_dispenv.screen.h = 256;
    } else {
        g_dispenv.screen.h = 224;
    }

    g_dispenv.screen.y = 16;

    if ((flag & 4) == 0) {
        g_dispenv.disp.y = 0;
        g_dispenv.disp.h = 256;
    } else {
        g_dispenv.disp.h = 224;
        g_dispenv.disp.y = (g_dispenv.screen.h - 224) / 2;
    }

    if ((flag & 8) != 0) {
        PutDispEnv(&g_dispenv);
    }

    SetDefDrawEnv(&drawenv, 0, 0, 320, g_dispenv.disp.h);
    drawenv.isbg = 1;
    DG_SetRenderChanlDrawEnv(-1, &drawenv);
#endif
}

void DG_RenderPipeline_Init(void)
{
    DG_ClearChanlSystem(0);
    DG_ClearChanlSystem(1);
    DG_RenderPipeline(0);
    DG_RenderPipeline(1);
}

void DG_SwapFrame(void)
{
    int activeBuffer = GV_Clock;

#ifdef MGS_TURNTABLE
    /* ~11 s per revolution at this stage's frame rate: slow enough to study a
     * joint as it comes round, fast enough not to have to wait for it. */
    { extern int mgs_dbg_spin; mgs_dbg_spin = (mgs_dbg_spin + 32) & 4095; }
#endif

#ifdef __psyz
    {
        /* the frame loop keeps ticking but nothing reaches the GPU: print the
         * three values this function branches on */
        static int beat;
        beat++;
        if ((beat % 120) == 0)
        {
            extern unsigned mgs_dbg_drawotag;
            extern unsigned sotn_exeque_count;
            extern unsigned mgs_vblank_count;
            extern DG_CHANL DG_Chanls[];
            extern unsigned psyz_prof_exeq_us, psyz_prof_load_us,
                psyz_prof_load_n;
            extern unsigned psyz_prim_by_code[8];
            extern unsigned psyz_texel_zero, psyz_texel_ok, psyz_flat_px;
            extern unsigned mgs_dbg_objs_seen, mgs_dbg_objs_culled,
                mgs_dbg_models_culled, mgs_dbg_models_drawn;
            extern unsigned mgs_dbg_cull_sat, mgs_dbg_cull_off,
                mgs_dbg_clip_dist;
            printf("[cull] objs %u culled %u | models drawn %u culled %u\n",
                   mgs_dbg_objs_seen, mgs_dbg_objs_culled,
                   mgs_dbg_models_drawn, mgs_dbg_models_culled);
            /* saturated = the perspective divide overflowed, so the box says
             * nothing about where the object is and it is drawn anyway;
             * offscreen = a well-formed box that really is out of view */
            printf("[bbox] kept-saturated %u culled %u | H %u clip %d,%d..%d,%d\n",
                   mgs_dbg_cull_sat, mgs_dbg_cull_off, mgs_dbg_clip_dist,
                   DG_ClipMin[0], DG_ClipMin[1], DG_ClipMax[0], DG_ClipMax[1]);
            {
                /* When "kept-saturated" runs away, every object's projection is
                 * overflowing and the whole scene gets drawn at whatever size
                 * the broken divide produced -- a bright, flickering wall of
                 * geometry rather than a light. The question is whether the
                 * CAMERA caused it, and the eye matrix answers that directly
                 * without needing a header out of game/: its translation is
                 * where the view sits, its diagonal says whether the rotation
                 * is still sane. camera.c can leave GM_CameraList indexed at -1
                 * when no trigger matches ("change camera -1" in the log), so
                 * this is the number to line that message up against. If the
                 * eye holds still while saturation explodes, the camera is
                 * innocent and the fault is in the projection itself. */
                /* Print the camera the GAME holds, not the view matrix.
                 * eye_inv.t is NOT a position: this file builds it by applying
                 * eye_inv's rotation to a forward vector, so it is a rotated
                 * direction. Reading it as "where the camera is" produced a
                 * confident story about the view flying upward that the data
                 * never supported. GM_Camera.position and .target are the real
                 * thing, and printing both separates three different faults:
                 * the camera ran away, its target ran away, or neither did and
                 * the saturation comes from the geometry instead. */
                extern GM_CameraSystemWork GM_Camera;
                printf("[cam] pos %d,%d,%d target %d,%d,%d\n",
                       GM_Camera.position.vx, GM_Camera.position.vy,
                       GM_Camera.position.vz, GM_Camera.target.vx,
                       GM_Camera.target.vy, GM_Camera.target.vz);
            }
            mgs_dbg_cull_sat = 0; mgs_dbg_cull_off = 0;
            mgs_dbg_objs_seen = 0; mgs_dbg_objs_culled = 0;
            mgs_dbg_models_drawn = 0; mgs_dbg_models_culled = 0;
            extern unsigned psyz_tri_off, psyz_tri_tiny, psyz_tri_ok, psyz_tri_huge;
            /* oversize = wider than 1023 or taller than 511, which the PSX GPU
             * refuses outright. These are polygons whose projection saturated
             * because they cross behind the camera; drawing them is what put a
             * moving white sheet over the dock. */
            printf("[tri] offscreen %u subpixel %u oversize %u drawable %u\n",
                   psyz_tri_off, psyz_tri_tiny, psyz_tri_huge, psyz_tri_ok);
            psyz_tri_off = 0;
            psyz_tri_tiny = 0;
            psyz_tri_huge = 0;
            psyz_tri_ok = 0;
            printf("[texel] transparent %u drawn %u flat %u\n",
                   psyz_texel_zero, psyz_texel_ok, psyz_flat_px);
            psyz_texel_zero = 0;
            psyz_texel_ok = 0;
            psyz_flat_px = 0;
            printf("[prims] 2x %u 3x %u 4x %u 5x %u 6x %u 7x %u\n",
                   psyz_prim_by_code[2], psyz_prim_by_code[3],
                   psyz_prim_by_code[4], psyz_prim_by_code[5],
                   psyz_prim_by_code[6], psyz_prim_by_code[7]);
            printf("[swap] f%d vbl %u q %d/%d %d/%d drawot %u exeq %u "
                   "raster_us %u load_us %u loads %u\n",
                   beat, mgs_vblank_count,
                   DG_Chanls[0].objs_index, DG_Chanls[0].prim_index,
                   DG_Chanls[1].objs_index, DG_Chanls[1].prim_index,
                   mgs_dbg_drawotag, sotn_exeque_count,
                   psyz_prof_exeq_us, psyz_prof_load_us, psyz_prof_load_n);
            psyz_prof_exeq_us = 0;
            psyz_prof_load_us = 0;
            psyz_prof_load_n = 0;
        }
    }
#endif

    if ((GV_PauseLevel & GV_PAUSE_READERROR) != 0 || DG_UnDrawFrameCount > 0)
    {
#ifdef __psyz
        {
            /* Which of the two conditions is holding the screen blank? They
             * mean very different things: READERROR is "the disc is failing"
             * (set from a streamed-audio underrun or a CD task error), while
             * UnDrawFrameCount is "something else owns the screen" -- the
             * movie player parks it at 0x7fff0000 while an FMV runs. */
            static unsigned n;
            if ((n++ % 240u) == 0u)
            {
                extern const char *mgs_dbg_undraw_src;
                printf("[skip] pause %02x readerr %d undraw %d set-by %s\n",
                       GV_PauseLevel,
                       (GV_PauseLevel & GV_PAUSE_READERROR) ? 1 : 0,
                       DG_UnDrawFrameCount, mgs_dbg_undraw_src);
            }
        }
#endif
        if (DG_CurrentBuffer < 0)
        {
            DG_CurrentBuffer = activeBuffer;
        }
        if ((GV_PauseLevel & GV_PAUSE_READERROR) == 0)
        {
            --DG_UnDrawFrameCount;
        }
    }
    else if (DG_CurrentBuffer < 0 || activeBuffer != DG_CurrentBuffer)
    {
        DISPENV *p = &g_dispenv;
#ifdef __psyz
        { static int b = 3; if (b > 0) { b--; printf("[swap] branch DRAW\n"); } }
#endif
        p->disp.x = gClipHeights_800AB960[activeBuffer];

        PutDispEnv(&g_dispenv);
#ifdef __psyz
        /* The panel is not a CRT: pushing a buffer costs a real 120 KB read
         * out of PSRAM, strided across rows, competing with the rasterizer on
         * the other core. Tell the scanout task that the picture changed so it
         * sends each frame once instead of on a fixed timer -- a scene that
         * takes four vblanks to draw was being sent twice. */
        { extern unsigned mgs_frame_seq; mgs_frame_seq++; }
#endif
        if (!DG_HikituriFlagOld)
        {
#ifdef __psyz
            { extern unsigned mgs_dbg_drawotag; mgs_dbg_drawotag++; }
#endif
            DG_DrawOTag(1 - activeBuffer);
#ifdef __psyz
            {
                /* Flush the queue NOW and count what actually landed in the
                 * buffer we just drew. If this is high but the panel is
                 * black, something wipes it afterwards; if it is zero, the
                 * drawing never reaches VRAM at all. */
                static int b = 6;
                if (b > 0)
                {
                    extern unsigned short* g_RawVram;
                    unsigned nz = 0, yy, xx, base = (1 - activeBuffer) * 320u;
                    b--;
                    DrawSync(0);
                    for (yy = 0; yy < 224; yy += 4)
                    {
                        for (xx = 0; xx < 320; xx += 4)
                        {
                            if (g_RawVram[yy * 1024u + base + xx]) { nz++; }
                        }
                    }
                    printf("[drawn] buffer %d (x %u) nonzero %u of 4480\n",
                           1 - activeBuffer, base, nz);
                }
            }
#endif
        }
        DG_CurrentBuffer = -1;
    }
    GV_ClearMemorySystem(activeBuffer);
    if (!DG_HikituriFlagOld)
    {
        GV_ClearMemorySystem(GV_NORMAL_MEMORY);
    }
    MENU_ResetSystem();
    DG_ClearChanlSystem(activeBuffer);
    DG_ClearTmpLight();
}

void DG_RenderFrame(void)
{
    DG_RenderPipeline(GV_Clock);
}

void DG_LookAt(DG_CHANL *chanl, SVECTOR *eye, SVECTOR *center, int clip_distance)
{
    VECTOR  forward;
    VECTOR  up;
    VECTOR  right;
    MATRIX *view;

    chanl->clip_distance = clip_distance;

    view = &chanl->eye;
    view->t[0] = eye->vx;
    view->t[1] = eye->vy;
    view->t[2] = eye->vz;

    forward.vx = (signed short)(((u_short)center->vx - (u_short)eye->vx));
    forward.vy = (signed short)(((u_short)center->vy - (u_short)eye->vy));
    forward.vz = (signed short)(((u_short)center->vz - (u_short)eye->vz));

    OuterProduct12(&DG_UpVector, &forward, &right);

    if (!right.vx && !right.vy && !right.vz)
    {
        right = DG_RightVector;
    }
    else
    {
        DG_RightVector = right;
    }

    VectorNormal(&right, &right);
    VectorNormal(&forward, &forward);

    OuterProduct12(&forward, &right, &up);

    view->m[0][0] = right.vx;
    view->m[0][1] = up.vx;
    view->m[0][2] = forward.vx;

    view->m[1][0] = right.vy;
    view->m[1][1] = up.vy;
    view->m[1][2] = forward.vy;

    view->m[2][0] = right.vz;
    view->m[2][1] = up.vz;
    view->m[2][2] = forward.vz;

    DG_TransposeMatrix(view, &chanl->eye_inv);

    forward.vx = -view->t[0];
    forward.vy = -view->t[1];
    forward.vz = -view->t[2];

    ApplyMatrixLV(&chanl->eye_inv, &forward, (VECTOR *)chanl->eye_inv.t);
}

void DG_AdjustOverscan(MATRIX *matrix)
{
    matrix->m[1][0] = (matrix->m[1][0] * 58) / 64;
    matrix->m[1][1] = (matrix->m[1][1] * 58) / 64;
    matrix->m[1][2] = (matrix->m[1][2] * 58) / 64;
    matrix->t[1] = (matrix->t[1] * 58) / 64;
}

void DG_Clip(RECT *clip_rect, int dist)
{
    int x_tmp;
    int y_tmp;

    gte_SetGeomScreen(dist);

#ifdef __psyz
    {
        /* H decides where the perspective divide starts to overflow (anything
         * nearer than H/2), which is what the [bbox] counters are testing */
        extern unsigned mgs_dbg_clip_dist;
        mgs_dbg_clip_dist = (unsigned)dist;
    }
#endif

    x_tmp = clip_rect->x;
    DG_ClipMin[0] = x_tmp;
    DG_ClipMax[0] = clip_rect->w + x_tmp - 1;

    y_tmp = clip_rect->y;
    DG_ClipMin[1] = y_tmp;
    DG_ClipMax[1] = clip_rect->h + y_tmp - 1;
}

void DG_ApplyMatrix(MATRIX *world, MATRIX *in)
{
    MATRIX out;

    gte_SetRotMatrix(world);

    gte_ldclmv(in);
    gte_rtir();
    gte_stclmv(&out);

    gte_ldclmv(&in->m[0][1]);
    gte_rtir();
    gte_stclmv(&out.m[0][1]);

    gte_ldclmv(&in->m[0][2]);
    gte_rtir();
    gte_stclmv(&out.m[0][2]);

    gte_SetTransMatrix(world);

    gte_ldlv0(in->t);
    gte_rt();
    gte_stlvnl(out.t);

    DG_AdjustOverscan(&out);

    gte_SetRotMatrix(&out);
    gte_SetTransMatrix(&out);
}

void DG_OffsetDispEnv(int offset)
{
    g_dispenv.screen.y += offset;
    g_dispenv.screen.h -= offset;
    PutDispEnv(&g_dispenv);
    g_dispenv.screen.y -= offset;
    g_dispenv.screen.h += offset;
}

void DG_ClipDispEnv(int x, int y)
{
    RECT screen;

    screen = g_dispenv.screen;
    g_dispenv.screen.x = 128 - x / 2;
    g_dispenv.screen.w = x;
    g_dispenv.screen.y = 120 - y / 2;
    g_dispenv.screen.h = y;
    PutDispEnv(&g_dispenv);
    g_dispenv.screen = screen;
}

void DG_DisableClipping(void)
{
    DRAWENV drawenv;

    DG_InitDrawEnv(&drawenv,
        g_dispenv.disp.x, g_dispenv.disp.y,
        g_dispenv.disp.w, g_dispenv.disp.h);
    PutDrawEnv(&drawenv);
}

void DG_FadeScreen(int amount)
{
    DR_TPAGE tpage;
    TILE     tile;

    DG_DisableClipping();

    setDrawTPage(&tpage, 1, 1, GetTPage(0, 2, 0, 0));
    DrawPrim(&tpage);

    tile.x0 = 0;
    tile.y0 = 0;
    tile.w = FRAME_WIDTH;
    tile.h = FRAME_HEIGHT;
    LSTORE(amount << 16 | amount << 8 | amount, &tile.r0);
    setTile(&tile);
    setSemiTrans(&tile, 1);
    DrawPrim(&tile);
}

// guessed function name
DISPENV *DG_GetDisplayEnv(void)
{
    return &g_dispenv;
}
