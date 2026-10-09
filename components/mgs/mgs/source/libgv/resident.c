#include "libgv.h"
#include <stdio.h>
#include "common.h"

extern unsigned char *StageCharacterEntries;

unsigned char *SECTION(".sbss") GV_ResidentMemoryBottom;
STATIC int     SECTION(".sbss") dword_800AB944;

#ifdef MGS_ESPBOX
void *GV_ResidentAreaBottom;    /* mgs_main_ram is a pointer here: set on init */
#else
void *GV_ResidentAreaBottom = RESIDENT_BOTTOM;
#endif

void GV_InitResidentMemory( void )
{
#ifdef MGS_ESPBOX
    if ( !GV_ResidentAreaBottom ) GV_ResidentAreaBottom = RESIDENT_BOTTOM;
#endif
    GV_ResidentMemoryBottom = GV_ResidentAreaBottom;
}

void GV_SaveResidentTop( void )
{
    GV_ResidentAreaBottom = GV_ResidentMemoryBottom;
}

/**
 * @brief   Reserves a block of memory from the bottom
 *          of the resident memory area.
 *
 * @param   size    The size of the memory block to reserve.
 *
 * @return  pointer to the new resident memory bottom
 */
void *GV_AllocResidentMemory( long size )
{
#ifdef DEV_EXE
    // linker-defined symbol
    extern unsigned char _bss_orgend[];
#endif

    // Align the size to 4 bytes
    size = (size + 3) & ~3;

    // decrement the bottom of the resident memory
    GV_ResidentMemoryBottom -= size;

#if defined(__psyz)
    {
        /* the reservation's floor is the real limit; StageCharacterEntries is a
         * native link-time address here and comparing against it is noise */
#ifdef MGS_ESPBOX
        extern unsigned char* mgs_main_ram;   /* allocated at start (esp-box-emu) */
#else
        extern unsigned char mgs_main_ram[];
#endif
        if (GV_ResidentMemoryBottom < mgs_main_ram)
        {
            printf("Resident Memory Over !!\n");
        }
    }
    return GV_ResidentMemoryBottom;
#endif
#ifdef DEV_EXE
    // dev_exe has to compare to _bss_orgend since the overlay base pointer
    // used by the OG code will be pointing somewhere in the .data section.
    if (GV_ResidentMemoryBottom < _bss_orgend)
#else
    // BUG: the overlay can potentially be alloc'd over with no warning.
    if (GV_ResidentMemoryBottom < StageCharacterEntries)
#endif
    {
        printf("Resident Memory Over !!\n");
    }

    return GV_ResidentMemoryBottom;
}
