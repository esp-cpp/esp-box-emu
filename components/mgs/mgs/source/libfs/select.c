#include "libfs.h"

#include <stdio.h>
#include <sys/types.h>
#include <libapi.h>
#include <libcd.h>

#include "common.h"
#include "libsio/isio.h"

void FS_StartDaemon( void )
{
#ifdef __psyz
    printf( "[fsd] SetMem\n" );
    SetMem(2);
    printf( "[fsd] FS_CDInit\n" );
    FS_CDInit();
    printf( "[fsd] sio_output_stop\n" );
    sio_output_stop();
    printf( "[fsd] done\n" );
#else
    SetMem(2);          // Set effective memory to 2MB (DTL-H2X00 has 8MB)
    FS_CDInit();        // Init CD read system
    sio_output_stop();
#endif
}

void FS_CdStageProgBinFix( void )
{
    /* do nothing */
}

CdlCB CdReadCallback( CdlCB func )
{
    return NULL;
}

CdlCB CdReadMode( CdlCB func )
{
    /* do nothing */
}

typedef void ( *DslCB )( u_char, u_char* );
DslCB DsReadyCallback( DslCB func )
{
    printf( "DsReadyCallback %x\n", (u_int)func );
}

void DsDataCallback( void (*func)() )
{
    printf( "DsDataCallback %x\n", (u_int)func );
}

int PCinit(void)
{
    return -1;
}

int PCopen(const char *name, int flags, int perms)
{
    return 0;
}

int PCread(int fd, char *buff, int len)
{
    return 0;
}

int PCclose(int fd)
{
    return 0;
}
