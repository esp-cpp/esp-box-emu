#include "libfs.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <libcd.h>

#include "common.h"
#include "mts/mts.h"        // for mts_wait_vbl
#include "libgv/libgv.h"    // for GV_xxxMemory

/*---------------------------------------------------------------------------*/

#define FS_DIRNAME_MAX  8
#define FS_STAGEDIR_MAX 170     // ((FS_SECTOR_SIZE - 4) / sizeof(FS_DIR_ENTRY))

typedef struct _FS_DIR_ENTRY {
    char    name[FS_DIRNAME_MAX];   // ASCII stage name
    int     offset;                 // in number of sectors
} FS_DIR_ENTRY;

typedef struct _FS_DIR_INFO {
    int             pos;            // LBA position of the stage archive
    int             table_size;     // size of the directory table
    int             n_stages;       // number of stage directories
    FS_DIR_ENTRY   *table_buf;      // in-memory copy of the dir table
} FS_DIR_INFO;

STATIC FS_DIR_INFO fs_dir_info = {};

// NOTE: This code registers a single DIR-format archive and keeps it for
// the full duration of the game's runtime. This is only cleared at shutdown,
// since there are no deinitialization routines for LibFS's position tables.
//
// Theoretically, if there were more than one DIR-format archives present on
// the CD-ROM, the game could dynamically select which to register at startup,
// however, the program is currently hard-coded to load only "STAGE.DIR".

/*---------------------------------------------------------------------------*/

static int CdStageReadCallback(CDBIOS_TASK *task)
{
    unsigned int size, rounded;

    if (task->sectors_delivered == 0)
    {
        size = *(unsigned int *)task->buffer;
        rounded = (size + 3) / 4;

        task->size = rounded;
        task->remaining = rounded - 512;

        fs_dir_info.table_size = size;
    }
    return 1;
}

/**
 * @brief       Registers the DIR archive to read from.
 *
 * @note        Because the dir entry table is expected to fit within
 *              the first 2048 bytes (1 sector) of the file, archives
 *              can contain a maximum of 170 stage entries.
 *
 *              Therefore, the the maximum size of the entry table is
 *              2040 bytes (sizeof(FS_DIR_ENTRY) * FS_STAGEDIR_MAX).
 *
 * @param[out]  buffer      temporary work buffer
 * @param[in]   sector      starting LBA position of the DIR archive
 */
void FS_CdStageFileInit(void *buffer, int sector)
{
    int table_size;

    fs_dir_info.pos = sector;
    CDBIOS_ReadRequest(buffer, sector, FS_SECTOR_SIZE, &CdStageReadCallback);

    while (CDBIOS_ReadSync() > 0)
    {
        mts_wait_vbl(1);
    }

    table_size = fs_dir_info.table_size;

    if (fs_dir_info.table_buf == NULL)
    {
        fs_dir_info.table_buf = GV_AllocResidentMemory(table_size);
    }

    printf("%X %X %d\n",
        (unsigned int)buffer + 4,               // src address
        (unsigned int)fs_dir_info.table_buf,    // dest address
        table_size);                            // size

    GV_CopyMemory((char *)buffer + 4, fs_dir_info.table_buf, table_size);

    fs_dir_info.n_stages = table_size / sizeof(FS_DIR_ENTRY);

#ifdef __psyz
    {
        /* The whole stage system hangs off this table: if it comes out empty
         * every FS_LoadStageRequest answers "NOT FOUND" and the game idles
         * forever with nothing to load. Print what was actually parsed. */
        int k;
        printf("[dir] table_size %d -> %d stages, base sector %d\n",
               table_size, fs_dir_info.n_stages, fs_dir_info.pos);
        for (k = 0; k < fs_dir_info.n_stages && k < 8; k++)
        {
            printf("[dir]   %-8.8s at %d\n", fs_dir_info.table_buf[k].name,
                   fs_dir_info.table_buf[k].offset);
        }
    }
#endif
}

/**
 * Gets the starting offset (logical block address) of a requested
 * stage block within the registered DIR archive.
 *
 * @param       dirname     name of the requested stage
 *
 * @return      LBA position of the requested stage block.
 *              Returns -1 if the stage is not found.
 */
int FS_CdGetStageFileTop(char *dirname)
{
    FS_DIR_ENTRY *dir;
    int i;

    dir = fs_dir_info.table_buf;

#ifdef __psyz
    /* The table parses correctly at init time, so if a lookup still fails the
     * interesting question is what the table looks like NOW -- the resident
     * heap it lives in is rewound by the game manager after FS starts up. */
    {
        int k;
        if (0) printf("[dir?] want '%s' (%d,%d,%d,%d) among %d at %p\n", dirname,
               dirname[0], dirname[1], dirname[2], dirname[3],
               fs_dir_info.n_stages, (void*)dir);
        for (k = 0; k < fs_dir_info.n_stages && k < 8; k++)
        {
            if (0) printf("[dir?]   [%d] '%-8.8s' (%d,%d,%d,%d) at %d\n", k,
                   dir[k].name, dir[k].name[0], dir[k].name[1], dir[k].name[2],
                   dir[k].name[3], dir[k].offset);
        }
    }
#endif

    for (i = fs_dir_info.n_stages; i > 0; i--)
    {
        if (strncmp(dir->name, dirname, FS_DIRNAME_MAX) == 0)
        {
            return dir->offset + fs_dir_info.pos;
        }
        dir++;
    }
    return -1;
}
