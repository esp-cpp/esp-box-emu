#include "doom.hpp"

#include "shared_memory.h"

extern "C" {
#include <prboom/doomtype.h>
#include <prboom/doomstat.h>
#include <prboom/doomdef.h>
#include <prboom/d_main.h>
#include <prboom/d_net.h>
#include <prboom/g_game.h>
#include <prboom/i_system.h>
#include <prboom/i_video.h>
#include <prboom/i_sound.h>
#include <prboom/i_main.h>
#include <prboom/m_argv.h>
#include <prboom/m_fixed.h>
#include <prboom/m_menu.h>
#include <prboom/m_misc.h>
#include <prboom/r_draw.h>
#include <prboom/r_fps.h>
#include <prboom/s_sound.h>
#include <prboom/st_stuff.h>
#include <prboom/mus2mid.h>
#include <prboom/midifile.h>
#include <prboom/oplplayer.h>

#include <prboom/dbopl.h>
#include <prboom/opl.h>
#include <prboom/r_plane.h>

#include <prboom/hu_lib.h>
#include <prboom/hu_stuff.h>

// From dbopl
#if ( DBOPL_WAVE == WAVE_HANDLER ) || ( DBOPL_WAVE == WAVE_TABLELOG )
extern Bit16u *ExpTable;
#endif
#if ( DBOPL_WAVE == WAVE_HANDLER )
//PI table used by WAVEHANDLER
extern Bit16u *SinTable;
#endif
extern Bit16s *WaveTable; // [ 8 * 512 ];
#if ( DBOPL_WAVE == WAVE_TABLEMUL )
extern Bit16u *MulTable; // [ 384 ];
#endif

extern Bit8u *KslTable; // [ 8 * 16 ];
extern Bit8u *TremoloTable; // [ TREMOLO_TABLE ];
extern Bit16u *ChanOffsetTable; // [32];
extern Bit16u *OpOffsetTable; // [64];

// from doomstat.h / g_game.c
extern player_t *players; // [MAXPLAYERS];
extern byte *gamekeydown; // [NUMKEYS];
extern char *doom_player_msg; // [MAX_MESSAGE_SIZE]

// from info.c
extern mobjinfo_t *mobjinfo; // [NUMMOBJTYPES];
extern const mobjinfo_t init_mobjinfo[NUMMOBJTYPES];

// from m_cheat.c/h
extern struct cheat_s *cheat;
extern const struct cheat_s init_cheat[];
extern size_t init_cheat_bytes;

// from p_mobj.c
extern mapthing_t *itemrespawnque;
extern int *itemrespawntime;
extern statenum_t *seenstate_tab; // [NUMSTATES]

// from sounds.c/h
extern musicinfo_t *S_music;
extern const size_t s_music_bytes;
extern const musicinfo_t init_S_music[];
extern sfxinfo_t *S_sfx;
extern const sfxinfo_t init_S_sfx[];
extern const size_t s_sfx_bytes;

// from st_stuff.c/h
extern patchnum_t *tallnum; // [10];
extern patchnum_t *shortnum; // [10];
extern patchnum_t *keys; // [NUMCARDS+3];
extern patchnum_t *faces; // [ST_NUMFACES];
extern patchnum_t *arms; // [6][2];

// from r_things.c/h
extern int *negonearray; // [MAX_SCREENWIDTH];        // killough 2/8/98: // dropoff overflow
extern int *screenheightarray; // [MAX_SCREENWIDTH];  // change to MAX_* // dropoff overflow
extern spriteframe_t *sprtemp; // [MAX_SPRITE_FRAMES]; [29]

// from opl.c
extern Chip *opl_chip;

// from r_plane.c/h
extern visplane_t **visplanes; // [MAXVISPLANES];   // killough
extern fixed_t *cachedheight; // [MAX_SCREENHEIGHT];
extern fixed_t *cacheddistance; // [MAX_SCREENHEIGHT];
extern fixed_t *cachedxstep; // [MAX_SCREENHEIGHT];
extern fixed_t *cachedystep; // [MAX_SCREENHEIGHT];
extern int *spanstart; // [MAX_SCREENHEIGHT];                // killough 2/8/98
extern int *floorclip; // [MAX_SCREENWIDTH];
extern int *ceilingclip; // [MAX_SCREENWIDTH];
extern fixed_t *yslope; // [MAX_SCREENHEIGHT];
extern fixed_t *distscale; // [MAX_SCREENWIDTH];

// from hu_stuff.c/h
extern patchnum_t *hu_font; // font[HU_FONTSIZE];
extern patchnum_t *hu_font2; // [HU_FONTSIZE];
extern patchnum_t *hu_fontk; // [HU_FONTSIZE];//jff 3/7/98 added for graphic key indicators
extern patchnum_t *hu_msgbg; // [9];          //jff 2/26/98 add patches for message background
extern char *hud_coordstrx; // [ 32];
extern char *hud_coordstry; // [ 32];
extern char *hud_coordstrz; // [ 32];
extern char *hud_ammostr; // [ 80];
extern char *hud_healthstr; // [ 80];
extern char *hud_armorstr; // [ 80];
extern char *hud_weapstr; // [ 80];
extern char *hud_keysstr; // [ 80];
extern char *hud_gkeysstr; // [ 80]; //jff 3/7/98 add support for graphic key display
extern char *hud_monsecstr; // [ 80];
extern char *chatchars; // [QUEUESIZE]; [128]
extern hu_textline_t  *w_title;
extern hu_stext_t     *w_message;
extern hu_itext_t     *w_chat;
extern hu_itext_t     *w_inputbuffer; // [MAXPLAYERS];
extern hu_textline_t  *w_coordx; //jff 2/16/98 new coord widget for automap
extern hu_textline_t  *w_coordy; //jff 3/3/98 split coord widgets automap
extern hu_textline_t  *w_coordz; //jff 3/3/98 split coord widgets automap
extern hu_textline_t  *w_ammo;   //jff 2/16/98 new ammo widget for hud
extern hu_textline_t  *w_health; //jff 2/16/98 new health widget for hud
extern hu_textline_t  *w_armor;  //jff 2/16/98 new armor widget for hud
extern hu_textline_t  *w_weapon; //jff 2/16/98 new weapon widget for hud
extern hu_textline_t  *w_keys;   //jff 2/16/98 new keys widget for hud
extern hu_textline_t  *w_gkeys;  //jff 3/7/98 graphic keys widget for hud
extern hu_textline_t  *w_monsec; //jff 2/16/98 new kill/secret widget for hud
extern hu_mtext_t     *w_rtext;  //jff 2/26/98 text message refresh widget

// from r_draw.c/h
extern byte           *byte_tempbuf; // [MAX_SCREENHEIGHT * 4];
#ifndef NOTRUECOLOR
extern unsigned short *short_tempbuf; // [MAX_SCREENHEIGHT * 4];
extern unsigned int   *int_tempbuf; // ol[MAX_SCREENHEIGHT * 4];
#endif
extern int *fuzzoffset; // [FUZZTABLE];

// from f_wipe.c
extern int *y_lookup; // [MAXSCREENWIDTH]

// from r_main.c/r_state.h
extern angle_t *xtoviewangle; // [MAX_SCREENWIDTH+1];   // killough 2/8/98

// from r_bsp.c
extern byte *solidcol; // [MAX_SCREENWIDTH];
} // extern "C"

void doom_init_shared_memory() {
    // needed for dbopl
#if ( DBOPL_WAVE == WAVE_HANDLER ) || ( DBOPL_WAVE == WAVE_TABLELOG )
    ExpTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 256);
#endif
#if ( DBOPL_WAVE == WAVE_HANDLER )
    SinTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 512);
#endif
    WaveTable = (Bit16s *)shared_malloc(sizeof(Bit16s) * 8 * 512);
#if ( DBOPL_WAVE == WAVE_TABLEMUL )
    MulTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 384);
#endif
    KslTable = (Bit8u *)shared_malloc(sizeof(Bit8u) * 8 * 16);
    TremoloTable = (Bit8u *)shared_malloc(sizeof(Bit8u) * TREMOLO_TABLE);
    ChanOffsetTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 32);
    OpOffsetTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 64);

    // needed for doomstat / g_game
    players = (player_t *)shared_malloc(sizeof(player_t) * MAXPLAYERS);
    gamekeydown = (byte *)shared_malloc(sizeof(byte) * NUMKEYS);
    doom_player_msg = (char*)shared_malloc(MAX_MESSAGE_SIZE);

    // needed for info
    mobjinfo = (mobjinfo_t *)shared_malloc(sizeof(mobjinfo_t) * NUMMOBJTYPES);
    memcpy(mobjinfo, init_mobjinfo, sizeof(mobjinfo_t) * NUMMOBJTYPES);

    // needed for m_cheat
    cheat = (struct cheat_s *)shared_malloc(init_cheat_bytes);
    memcpy(cheat, init_cheat, init_cheat_bytes);

    // needed for p_mobj.c
    itemrespawnque = (mapthing_t *)shared_malloc(sizeof(mapthing_t) * ITEMQUESIZE);
    itemrespawntime = (int *)shared_malloc(sizeof(int) * ITEMQUESIZE);
    seenstate_tab = (statenum_t *)shared_malloc(sizeof(statenum_t)*NUMSTATES);

    // needed for sounds.c/h
    S_music = (musicinfo_t *)shared_malloc(s_music_bytes);
    memcpy(S_music, init_S_music, s_music_bytes);
    S_sfx = (sfxinfo_t *)shared_malloc(s_sfx_bytes);
    memcpy(S_sfx, init_S_sfx, s_sfx_bytes);
    S_sfx[sfx_chgun].link = &S_sfx[sfx_pistol];

    // needed for st_stuff.c/h
    tallnum = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * 10);
    shortnum = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * 10);
    keys = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * (NUMCARDS + 3));
    faces = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * ST_NUMFACES);
    arms = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * 6 * 2);

    // needed for r_things.c/h
    negonearray = (int *)shared_malloc(sizeof(int) * MAX_SCREENWIDTH); // killough 2/8/98: // dropoff overflow
    screenheightarray = (int *)shared_malloc(sizeof(int) * MAX_SCREENWIDTH); // change to MAX_* // dropoff overflow
    sprtemp = (spriteframe_t *)shared_malloc(sizeof(spriteframe_t) * 29); // [29]

    // needed for opl.c
    opl_chip = (Chip *)shared_malloc(sizeof(Chip));

    // needed for r_plane.c/h
    visplanes = (visplane_t **)shared_malloc(sizeof(visplane_t *) * MAXVISPLANES);
    cachedheight = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    cacheddistance = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    cachedxstep = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    cachedystep = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    spanstart = (int *)shared_malloc(sizeof(int) * MAX_SCREENHEIGHT);
    floorclip = (int *)shared_malloc(sizeof(int) * MAX_SCREENWIDTH);
    ceilingclip = (int *)shared_malloc(sizeof(int) * MAX_SCREENWIDTH);
    yslope = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    distscale = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENWIDTH);

    // needed for hu_stuff.c/h
    hu_font = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * HU_FONTSIZE);
    hu_font2 = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * HU_FONTSIZE);
    hu_fontk = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * HU_FONTSIZE);
    hu_msgbg = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * 9);
    hud_coordstrx = (char *)shared_malloc(32);
    hud_coordstry = (char *)shared_malloc(32);
    hud_coordstrz = (char *)shared_malloc(32);
    hud_ammostr = (char *)shared_malloc(80);
    hud_healthstr = (char *)shared_malloc(80);
    hud_armorstr = (char *)shared_malloc(80);
    hud_weapstr = (char *)shared_malloc(80);
    hud_keysstr = (char *)shared_malloc(80);
    hud_gkeysstr = (char *)shared_malloc(80); //jff 3/7/98 add support for graphic key display
    hud_monsecstr = (char *)shared_malloc(80);
    chatchars = (char *)shared_malloc(128); // [128]
    w_title = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_message = (hu_stext_t *)shared_malloc(sizeof(hu_stext_t));
    w_chat = (hu_itext_t *)shared_malloc(sizeof(hu_itext_t));
    w_inputbuffer = (hu_itext_t *)shared_malloc(sizeof(hu_itext_t) * MAXPLAYERS);
    w_coordx = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_coordy = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_coordz = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_ammo = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_health = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_armor = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_weapon = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_keys = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_gkeys = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t)); //jff 3/7/98 graphic keys widget for hud
    w_monsec = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_rtext = (hu_mtext_t *)shared_malloc(sizeof(hu_mtext_t));

    // needed for r_draw.c/h
    byte_tempbuf = (byte*)shared_malloc(MAX_SCREENHEIGHT * 4);
#ifndef NOTRUECOLOR
    short_tempbuf = (short*)shared_malloc(MAX_SCREENHEIGHT * 4 * sizeof(short));
    int_tempbuf = (int*)shared_malloc(MAX_SCREENHEIGHT * 4 * sizeof(int));
#endif
    fuzzoffset = (int *)shared_malloc(sizeof(int) * FUZZTABLE); // [FUZZTABLE];

    // needed for f_wipe.c
    y_lookup = (int*)shared_malloc(MAX_SCREENWIDTH * sizeof(int));

    // needed for r_main.c/r_state.h
    xtoviewangle = (angle_t*)shared_malloc((MAX_SCREENWIDTH+1) * sizeof(angle_t));

    // needed for r_bsp.c
    solidcol = (byte *)shared_malloc(MAX_SCREENWIDTH);
}
