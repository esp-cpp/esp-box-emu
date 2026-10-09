#define __MENU_MENUMAN_C__
/* Draw the codec conversations (the Colonel calls) rather than answering them
 * invisibly.
 *
 * Held at 0. Turning it on is a one-flag experiment and it has been run: the
 * face group asks for 417792 bytes -- 408 KB out of GV_PACKET_MEMORY0, not the
 * 150 KB the code's own comment suggests -- and the allocation fails on this
 * board. With the real module in charge and the headless drain off, the call
 * is then never answered at all, so the story stops: s00a stays on its first
 * pass and the dock is never built (4 objects queued instead of 3000).
 *
 * The memory half is now solved -- the group loads into a PSRAM reserve (see
 * port/psyz_port.c) and 417792 bytes arrive without a crash. What is still
 * missing is the INDEX: the start sector handed to FS_LoadFileRequest comes
 * back as 15658670, and FACE.DAT is 3.5 MB, about 1700 sectors. That value is
 * uninitialised memory, so the table mapping a call to its position in the
 * file has not been read yet. Faces parsed: 0.
 *
 * Held at 0 until that index is loaded, because with the module in charge and
 * the headless drain off, a call that cannot draw is a call that is never
 * answered -- and then s00a never gets past its first pass. */
#ifndef MGS_CODEC_SCREEN
#define MGS_CODEC_SCREEN 0
#endif

#include "menuman.h"

#include <stdio.h>
#include "common.h"
#include "radar.h"
#include "mts/mts.h" // for fprintf
#include "libgv/libgv.h"
#include "libdg/libdg.h"
#include "libgcl/libgcl.h"
#include "game/game.h"

extern MenuWork      gMenuWork_800BD360;
extern unsigned char menu_primbuffers[2][8192];

extern int MENU_PrimUse;

extern GV_PAD *GM_CurrentPadData;
extern GV_PAD        *GM_CurrentPadData;

void menu_texture_init_8003CC94(MenuWork *work);
void menu_radar_init_8003B474(MenuWork *work);
void menu_radio_init(MenuWork *work);
void menu_item_init_8003CBF0(MenuWork *work);
void menu_weapon_init_8003EC2C(MenuWork *work);
void menu_life_init_8003F7E0(MenuWork *work);
void menu_number_init(MenuWork *work);
void menu_jimaku_init(MenuWork *work);

TInitKillFn gMenuInitFns_8009E290[] = {
    menu_texture_init_8003CC94,
    menu_radar_init_8003B474,
    menu_radio_init,
    menu_item_init_8003CBF0,
    menu_weapon_init_8003EC2C,
    menu_life_init_8003F7E0,
    menu_number_init,
    menu_jimaku_init,
    NULL};

void menu_radar_kill_8003B554(MenuWork *work);
void menu_radio_kill(MenuWork *work);
void menu_item_kill_8003CC74(MenuWork *work);
void menu_weapon_kill_8003ECAC(MenuWork *work);
void menu_life_kill_8003F838(MenuWork *work);
void menu_number_kill(MenuWork *work);

TInitKillFn gMenuKillFns_8009E2B4[] = {
    menu_radar_kill_8003B554,
    menu_radio_kill,
    menu_item_kill_8003CC74,
    menu_weapon_kill_8003ECAC,
    menu_life_kill_8003F838,
    menu_number_kill,
    NULL};

MenuPrim menu_prim = {0, 0, 0, {0, 0}};
TextConfig gMenuTextConfig_8009E2E4 = {0, 0, 0, 0x64808080};

void menuman_act_800386A4(MenuWork *work)
{
    OT_TYPE *pOtStart;
    /* This used to return immediately under __psyz, which switched off the
     * whole HUD -- the soliton radar included. The reason given was that some
     * drawer still emitted hand-packed PSX tags that desynced the GPU stream,
     * but nothing in source/menu/ builds a tag that way under __psyz any more:
     * the only two such idioms left are in an #else arm. The stub outlived the
     * problem it was written for. */
    int     idx_as_flag;
    int     field_28_flags;
    int     i;

    pOtStart = (&menu_prim)->ot;
    work->field_24_pInput = &GM_CurrentPadData[2];
    menu_jimaku_act(work, pOtStart);
    if ( ( !(GV_PauseLevel & GV_PAUSE_PAUSE) && (GM_LoadComplete > 0) ) &&
         ( !GM_LoadRequest ) )
    {
        idx_as_flag = 1;
        if (GM_GameStatus >= 0)
        {
#ifdef __psyz
            /* MGS_CODEC_SCREEN picks which of the two answers a call.
             *
             * 0: the headless drain -- marks the call answered and dispatches
             *    its procedure, so the story advances, but nothing is drawn.
             *    That is what shipped while the face path was known to crash.
             * 1: the real radio module, enabled in the mask below. It walks
             *    FACE.DAT and draws the conversation. Both at once would run
             *    each call's procedure twice, so it is one or the other. */
            if (!MGS_CODEC_SCREEN)
            {
                void MENU_RadioDrainHeadless(void);
                MENU_RadioDrainHeadless();
            }
#endif
            field_28_flags = work->field_28_flags;
#ifdef __psyz
            /* Bring the HUD back one drawer at a time rather than all eight.
             *
             * Restoring the lot at once crashed immediately, but not in a
             * drawer: MENU_RADIO's codec task walks FACE.DAT and took a null
             * pointer in radioanim.c:145. That is a whole separate subsystem
             * (the codec faces) and nothing the player needs to move around,
             * so it stays off while the radar -- which is what tells you where
             * you are -- comes back. Widen this mask as each one is proven. */
            field_28_flags &= (1 << MENU_RADAR) |
                              (MGS_CODEC_SCREEN ? (1 << MENU_RADIO) : 0);
#endif
            for (i = 0; i < MENU_MODULE_MAX; i++)
            {
                if ((field_28_flags & idx_as_flag) != 0)
                {
                    work->field_2C_modules[i](work, pOtStart);
                }
                idx_as_flag *= 2;
            }
        }
    }

    addPrim(pOtStart, &work->field_4C_drawEnv[GV_Clock]);
}

void menuman_kill_800387E8(MenuWork *work)
{
    TInitKillFn *pIter;

    pIter = gMenuKillFns_8009E2B4;
    while (*pIter)
    {
        (*pIter)(work);
        pIter++;
    }

    menu_viewer_kill(work);
}

void menu_init_subsystems_8003884C(MenuWork *work)
{
    TInitKillFn *pIter;
    DRAWENV      drawEnv;

    work->field_2A_state = MENU_CLOSED;
    work->field_29 = 0;
    work->field_28_flags = 0;

    work->prim = &menu_prim;

    menu_prim.buf[0] = menu_primbuffers[0];
    menu_prim.buf[1] = menu_primbuffers[1];

    DG_InitDrawEnv(&drawEnv, 0, 0, FRAME_WIDTH, FRAME_HEIGHT);
    drawEnv.isbg = 0;
    drawEnv.tpage = 31;
    SetDrawEnv(&work->field_4C_drawEnv[0], &drawEnv);

    DG_InitDrawEnv(&drawEnv, 320, 0, FRAME_WIDTH, FRAME_HEIGHT);
    drawEnv.isbg = 0;
    drawEnv.tpage = 31;
    SetDrawEnv(&work->field_4C_drawEnv[1], &drawEnv);

    menu_rpk_init_8003DD1C("item");

    pIter = &gMenuInitFns_8009E290[0];
    while (*pIter)
    {
        (*pIter)(work);
        pIter++;
    }

    menu_viewer_init(work);
}

void menuman_init_80038954(void)
{
    GV_SetNamedActor(&gMenuWork_800BD360.actor, menuman_act_800386A4,
                     menuman_kill_800387E8, "menuman.c");
    menu_init_subsystems_8003884C(&gMenuWork_800BD360);
    MENU_InitRadioTable();
}

void menuman_Reset(void)
{
    MENU_ResetCall();
    MENU_ClearRadioTable();
    MENU_SetRadarScale(4096);
    MENU_SetRadarFunc(NULL);
    gMenuWork_800BD360.field_CC_radar_data.prev_mode = 0;
    gMenuWork_800BD360.field_CC_radar_data.counter = 0;
    gMenuWork_800BD360.field_2B = 0;
    gMenuWork_800BD360.field_1DC_menu_item.field_12_flashingAnimationFrame = 0;
    gMenuWork_800BD360.field_1F0_menu_weapon.field_12_flashingAnimationFrame = 0;
    menu_life_init_8003F7E0(&gMenuWork_800BD360);
}

void MENU_ResetTexture(void)
{
    menu_weapon_unknown_8003DEB0();
}

void MENU_StartDeamon(void)
{
    GV_InitActor(GV_ACTOR_MANAGER, &gMenuWork_800BD360.actor, NULL);
    GV_SetNamedActor(&gMenuWork_800BD360.actor, NULL, NULL, "menuman.c");
}

void menu_radio_update_helper_80038A6C(void)
{
    gMenuWork_800BD360.field_CC_radar_data.display_flag = 1;
}

void menu_radio_update_helper2_80038A7C(void)
{
    gMenuWork_800BD360.field_CC_radar_data.display_flag = 0;
}

void MENU_ResetSystem(void)
{
    if (menu_prim.next > menu_prim.end)
    {
        fprintf(-1, "!!!! MENU PRIM OVER !!!!\n");
    }

    MENU_PrimUse = menu_prim.next - menu_prim.buf[1 - GV_Clock];

    menu_prim.next = menu_prim.buf[GV_Clock];
    menu_prim.end = menu_prim.next + 0x2000;
    menu_prim.ot = DG_ChanlOTag(1);
    MENU_ResetText();
}

void MENU_Locate(int xpos, int ypos, int flags)
{
    TextConfig *config = &gMenuTextConfig_8009E2E4;

    config->xpos = xpos;
    config->ypos = ypos;
    config->flags = flags;
}

void MENU_Color(int r, int g, int b)
{
    unsigned int color;
    unsigned int code;
    TextConfig  *config = &gMenuTextConfig_8009E2E4;

    if ((config->flags & TextConfig_Flags_eSemiTransparent_20) != 0)
    {
        color = MAKE_RGB(r, g, b);
        code = ((GPU_CODE_SPRT | GPU_CODE_SEMITRANS) << RGBA_A_SHIFT);
    }
    else
    {
        color = MAKE_RGB(r, g, b);
        code = ((GPU_CODE_SPRT & ~GPU_CODE_SEMITRANS) << RGBA_A_SHIFT);
    }

    config->color = color | code;
}

void MENU_ResetText(void)
{
    TextConfig *config = &gMenuTextConfig_8009E2E4;
    config->color = MAKE_RGBA(128,128,128, GPU_CODE_SPRT);
    config->flags = 0;
}

void menu_Text_PrimUnknown_80038BB4(void)
{
    DR_TPAGE *pPrim; // $a0

    pPrim = (DR_TPAGE*)menu_prim.next;
    menu_prim.next += sizeof(DR_TPAGE);
    setDrawTPage(pPrim, 1, 1, getTPage(0, gMenuTextConfig_8009E2E4.flags >> 8, 960, 256));

    addPrim(menu_prim.ot, pPrim);
}

int MENU_Printf(const char *fmt, const char *str, int param_3, int param_4, int param_5)
{
    int          string_length;
    unsigned int free_space;
    char         string_buffer[64];

    if (menu_prim.next)
    {
        sprintf(string_buffer, (char *)fmt, str, param_3, param_4, param_5);
        free_space = menu_prim.end - menu_prim.next;
        string_length = strlen(string_buffer);
        if (string_length * 0x14 + 0x28U <= free_space)
        {
            if (gMenuTextConfig_8009E2E4.flags & TextConfig_Flags_eLargeFont_10)
            {
                _menu_number_draw_string2(&menu_prim, &gMenuTextConfig_8009E2E4,
                                          string_buffer);
            }
            else
            {
                _menu_number_draw_string(&menu_prim, &gMenuTextConfig_8009E2E4,
                                         string_buffer);
            }
            menu_Text_PrimUnknown_80038BB4();
        }
    }
    return gMenuTextConfig_8009E2E4.xpos;
}

int menu_draw_num(int number)
{
    if (!menu_prim.next)
    {

        return gMenuTextConfig_8009E2E4.xpos;
    }
    _menu_number_draw(&menu_prim, &gMenuTextConfig_8009E2E4, number);
    menu_Text_PrimUnknown_80038BB4();
    return gMenuTextConfig_8009E2E4.xpos;
}

MenuPrim *MENU_GetPrimInfo(void)
{
    return &menu_prim;
}

// specifically an enemy life bar
void MENU_DrawBar(int xpos, int ypos, int rest, int now, MENU_BAR_CONF *bconf)
{
    GM_GameStatus |= STATE_SHOW_LIFEBAR;
    draw_life_8003F464(&menu_prim, xpos, ypos, rest, now, 1024, bconf);
    menu_Text_PrimUnknown_80038BB4();
}

void MENU_DrawBar2(int ypos, int rest, int now, int max, MENU_BAR_CONF *bconf)
{
    draw_life_defaultX_8003F408(&menu_prim, ypos, rest, now, max, bconf);
}
