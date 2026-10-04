#include "libfs.h"

#include <stdio.h>
#include "common.h"
#include "mts/mts.h"        // for mts_wait_vbl
#include "libgv/libgv.h"    // for GV_xxx_MEMORY_TOP

extern int FS_DiskNum;

#include "file.cnf"     // defines fs_file_info

int FS_ResetCdFilePosition(void *buffer)
{
    int disk_num = FS_CdMakePositionTable(buffer, fs_file_info);
    printf("Position end\n");
    if (disk_num >= 0)
    {
        printf("DISK %d\n", disk_num);
        FS_CdStageFileInit(buffer, fs_file_info[FS_FILEID_STAGE].pos);
#ifdef __psyz
        /* With ZMOVIE.STR absent the index reads as zeroes, and the table
         * FS_MovieFileInit then builds lands straight on top of the stage
         * table -- its 8-byte records overwrote every other 12-byte stage
         * entry, which is why "init" was in the table at boot and gone by the
         * time anything looked for it. No movie file, no movie index. */
        {
            extern int Mgs_CdFilePresent(int fileid);
            if (Mgs_CdFilePresent(FS_FILEID_ZMOVIE))
            {
                FS_MovieFileInit(buffer, fs_file_info[FS_FILEID_ZMOVIE].pos);
            }
            else
            {
                printf("[cd] no ZMOVIE.STR: skipping the movie index\n");
            }
        }
#else
        FS_MovieFileInit(buffer, fs_file_info[FS_FILEID_ZMOVIE].pos);
#endif
    }
    else
    {
        printf("illegal DISK\n");
    }
    return disk_num;
}

void FS_CDInit(void)
{
#ifdef __psyz
    printf( "[cd] CDBIOS_Reset\n" );
    CDBIOS_Reset();
    printf( "[cd] ResetCdFilePosition\n" );
    FS_DiskNum = FS_ResetCdFilePosition(MEM_ADDR);
    printf( "[cd] disk %d, StreamCD\n", FS_DiskNum );
    FS_StreamCD();
    printf( "[cd] StreamTaskInit\n" );
    FS_StreamTaskInit();
    printf( "[cd] wait_vbl\n" );
    mts_wait_vbl(2);
    printf( "[cd] done\n" );
#else
    CDBIOS_Reset();
    FS_DiskNum = FS_ResetCdFilePosition(MEM_ADDR);
    FS_StreamCD();
    FS_StreamTaskInit();
    mts_wait_vbl(2);
#endif
}

void FS_LoadFileRequest(int fileno, int offset, int size, void *buffer)
{
    CDBIOS_ReadRequest(buffer, fs_file_info[fileno].pos + offset, size, NULL);
}

int FS_LoadFileSync(void)
{
    return CDBIOS_ReadSync();
}

void MakeFullPath(char *name, char *buffer)
{
    /* do nothing */
}
