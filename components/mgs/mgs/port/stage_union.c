/* One actor table for every stage this build can run, and the list of which
 * stages those are. GENERATED -- edit port/gen_stage_union.py, not this file.
 *
 * On the console each stage overlay carried its own _StageCharacterEntries[]
 * and only one overlay was resident, so the same symbol name never collided.
 * Natively there are no overlays: exactly one table can link, and whichever
 * stage it belonged to was the only one whose actors could spawn -- with
 * s07a's table linked, select1's script asked for the stage-select actor,
 * found nothing, and ended having drawn nothing. That was the black screen
 * after "end scenario".
 *
 * The per-stage split only existed to save PSX memory. Here the union is a
 * couple of hundred pointers, so link them all; GM_GetCharaID scans to
 * CHARA_END and finds whichever class the running stage asks for.
 *
 * Stages left out must never be entered: their tables still hold raw MIPS
 * addresses, and calling one jumps into whatever happens to sit at
 * 0x800dxxxx. Mgs_ResolveStage refuses them by name using the list at the
 * bottom. While only a handful of stages were packed into flash the missing
 * file was refusal enough, but a board reading the full 68.6 MB STAGE.DIR off
 * a card can reach all 96 and needs the explicit answer.
 */

#define DECLARE_NEWCHARA_PROTOS
#include "charalst.h"

CHARA _StageCharacterEntries[] = {
    /* select1 */
    CHARA_STAGESELECT,

    /* demosel */
    CHARA_DEMOSEL,

    /* roll */
    CHARA_ENDINGROLL,
    CHARA_MOVIE,
    CHARA_ED_TELOP,
    CHARA_B757_ED_TELOP,

    /* s07a */
    CHARA_ASIOTOKUN,
    CHARA_FADEIO,
    CHARA_COMMANDER,
    CHARA_WATCHER,
    CHARA_MERYL7,
    CHARA_DOOR,
    CHARA_WALL,
    CHARA_MIRROR,
    CHARA_ELEVPANEL,
    CHARA_DYNWALL,
    CHARA_TEXTURE,
    CHARA_CINEMA,
    CHARA_CAT_IN,

    /* s00a */
    CHARA_RIPPLES,
    CHARA_PADCONTROL,
    CHARA_PADVIBRATE,
    CHARA_SNEBREATH,
    CHARA_ENV_SOUND,
    CHARA_CAMERASHAKE,
    CHARA_PADDEMO,
    CHARA_ASIATOKUN,
    CHARA_SHAKEMODEL,
    CHARA_PATOLAMP,
    CHARA_WT_AREA,
    CHARA_SMOKE,
    CHARA_EMITTER,
    CHARA_ELEVATOR,
    CHARA_MOUSE,
    CHARA_RSURFACE,
    CHARA_TELOP,
    CHARA_BUBBLE,
    CHARA_FEWDAMAGE,

    /* s01a */
    CHARA_TOBCNT,
    CHARA_MOTIONSE,
    CHARA_DEMOCANCEL,
    CHARA_GASEFFECT,
    CHARA_DEMODOLL,
    CHARA_TRUCKTRAP,
    CHARA_CAMERA,
    CHARA_SPHERE,
    CHARA_BLINKTEX,
    CHARA_OBJECT,
    CHARA_SEARCHLIGHT,
    CHARA_SNOW,

    /* s02a */
    CHARA_INTRUDECAM,
    CHARA_BUB_D_SN,
    CHARA_PUT_OBJECT,
    CHARA_DUCTMOUSE,

    /* abst */
    CHARA_LOAD_DATA,
    CHARA_ABST,
    CHARA_ABST_DEMO1,
    CHARA_ABST_DEMO2,
    CHARA_AB_CH,

    /* camera */
    CHARA_JPEG,

    /* change */
    CHARA_CDCHANGE,

    /* d00a */
    CHARA_WT_VIEW,

    /* d03a */
    CHARA_UJI,

    /* d18a */
    CHARA_SNAKE18,
    CHARA_SMKTRGT,
    CHARA_WAKE,

    /* opening */
    CHARA_TEXSCROLL,

    /* option */
    CHARA_OPT,

    /* preope */
    CHARA_PREOPE,

    /* s02b */
    CHARA_SHUTTER,

    /* s02c */
    CHARA_KIKENKUN,
    CHARA_WSURFACE,
    CHARA_CENSOR,
    CHARA_GASDAMAGE,

    /* s03a */
    CHARA_FINDTRAP,
    CHARA_MOSAIC,
    CHARA_CAMERAGUN,
    CHARA_HIYOKO,
    CHARA_RADARPOINT,

    /* s03b */
    CHARA_TORTURE,
    CHARA_TR_BED,
    CHARA_TR_OCELOT,
    CHARA_BOXALL,

    /* s03c */
    CHARA_JOHNNY,
    CHARA_PRISONOTACON,
    CHARA_PRISONNINJA,
    CHARA_PRISONSNAKE,
    CHARA_PRISONSNAKE2,

    /* s04a */
    CHARA_DMYWALL,
    CHARA_DMYFLOOR,

    /* s04c */
    CHARA_REVOLVER04,
    CHARA_C4WIRE,
    CHARA_AT,
    CHARA_CAPE,
    CHARA_VOICESYS,
    CHARA_CLAYMORE,
    CHARA_LIFEUP,

    /* s07c */
    CHARA_MERYL72,
    CHARA_9FFD_2ND,

    /* s08a */
    CHARA_PANEL,
    CHARA_PILOTLAMP,
    CHARA_ELECDAMAGE,
    CHARA_ELECFLOOR,
    CHARA_BREAK_OBJECT,
    CHARA_PLASMA,
    CHARA_DEATHSPARK,
    CHARA_REDALERT,
    CHARA_GLASS,
    CHARA_FLR_SPA,

    /* s08c */
    CHARA_WALLSPARK,
    CHARA_BLOOD_POOL,
    CHARA_BLOOD_POOL2,

    /* s11c */
    CHARA_DYNFLOOR,
    CHARA_SCN_MARK,
    CHARA_FOG,
    CHARA_RASEN,
    CHARA_RASEN_EL,

    /* s11e */
    CHARA_ZAKO11ECOM,
    CHARA_ZAKO11E,

    /* s11g */
    CHARA_1787_HIND,
    CHARA_S11_OBJS,
    CHARA_11G_DEMO,

    /* s11i */
    CHARA_SNOWSTORM,
    CHARA_ZAKO11FCOM,
    CHARA_ZAKO11F,
    CHARA_HIND2,

    /* s12a */
    CHARA_WOLF,

    /* s12c */
    CHARA_DOG,

    /* s13a */
    CHARA_CRANE,
    CHARA_LIFT,
    CHARA_DOOR2,
    CHARA_FURNACE,

    /* s15b */
    CHARA_FALLSPLASH,

    /* s15c */
    CHARA_CONTAINER,
    CHARA_CROW,

    /* s16a */
    CHARA_KEY_ITEM,
    CHARA_WT_AREA2,
    CHARA_ITEM_DOT,

    /* s16d */
    CHARA_BELONG,

    /* s19b */
    CHARA_COUNTDOWN2,
    CHARA_JEEPSCROLL,
    CHARA_JEEPDRUM,
    CHARA_JEEP_EMY,

    /* select */
    CHARA_VIBEDITOR,

    /* sound */
    CHARA_SOUNDTEST,

    /* title */
    CHARA_OPEN,
    CHARA_VRWINDOW,
    CHARA_SAFETY,
    CHARA_FONTTEXT,
    CHARA_END
};

/* The stages whose actor table is represented above. "init" is not here: it is
 * the boot stage and spawns no actors of its own, so it is always allowed. */
const char *const Mgs_PlayableStages[] = {
    "abst", "camera", "change", "d00a", "d01a", "d03a",
    "d11c", "d16e", "d18a", "demosel", "ending", "opening",
    "option", "preope", "roll", "s00a", "s01a", "s02a",
    "s02b", "s02c", "s02d", "s03a", "s03b", "s03c",
    "s03e", "s04a", "s04c", "s06a", "s07a", "s07c",
    "s08a", "s08c", "s08cr", "s11c", "s11e", "s11g",
    "s11i", "s12a", "s12c", "s13a", "s15b", "s15c",
    "s16a", "s16b", "s16c", "s16d", "s19b", "s20a",
    "select", "select1", "select2", "select3", "select4", "selectd",
    "sound", "title",
    0
};
