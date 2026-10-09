#include "common.h"
#include "mts/mts.h"

#include "game.h"
#include "charadef.h"
#include "libgcl/libgcl.h"
#include "linkvar.h"

extern CHARA MainCharacterEntries[];    /* in main.c */
extern CHARA _StageCharacterEntries[];  /* only visible when built-in */

// force gp usage
void *SECTION(".sbss") StageCharacterEntries;

void GM_InitChara(void)
{
#if defined(DEV_EXE) || defined(__psyz)
    StageCharacterEntries = &_StageCharacterEntries[0];
#else
    extern void *mts_get_bss_tail(void);
    StageCharacterEntries = mts_get_bss_tail();
#endif
}

void GM_ResetChara(void)
{
#if !defined(DEV_EXE) && !defined(__psyz)
    CHARA *chara;

    chara = (CHARA *)StageCharacterEntries;
    // overwrite the first entry with the end-of-table marker
    *((int *)&chara->func) = 0;
    *((int *)&chara->class_id) = 0;
#endif
}

void *GM_GetChara(unsigned char *script)
{
    return GM_GetCharaID(GCL_StrToInt(script));
}

void *GM_GetCharaID(int chara_id)
{
    CHARA *chara;
    int    i;

    for (i = 0; i < 2; i++)
    {
        // First, search the built-in charas
        chara = &MainCharacterEntries[0];
        if (i != 0)
        {
            // chara_id wasn't found in the main binary's built-ins,
            // so now we'll search the stage overlay's chara table.
            chara = (CHARA *)StageCharacterEntries;
        }

        for (; chara->func != NULL; chara++)
        {
            if (chara->class_id == chara_id)
            {
                return chara->func;
            }
        }
    }

#ifdef __psyz
    {
        /* A class the stage asks for that this build cannot supply. Every one
         * of these is a thing that silently does not exist in the world -- the
         * water surface at the dock is the visible example: Snake starts under
         * it, so with the surface actor missing there is nothing above him but
         * the background colour. Naming the id makes it findable in
         * charalst.h, and whether it is missing from stage_union.c or simply
         * not decompiled. */
        static int budget = 24;
        if (budget > 0)
        {
            budget--;
            printf("[chara] class %04X NOT AVAILABLE\n", chara_id & 0xFFFF);
        }
    }
#endif
    return NULL;
}
