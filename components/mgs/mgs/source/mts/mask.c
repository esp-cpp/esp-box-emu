// I think this object was originally implemented in assembly.
//
// mask.obj in 5thMix's MTS builds have no source filename (as GCC inserts
// a .file directive in the assembly output) and PSY-Q object parsers usually
// complain about an unknown record type or some other bullshit.

void SetExMask()
{
    // unknown psyq-specific debug function ?
#ifndef __psyz
    __asm__("break 1030");
#else
    /* PSY-Q debugger trap; nothing to break into elsewhere */
#endif
}

void *mts_get_bss_tail(void)
{
    // linker-defined symbol
    extern unsigned char _bss_orgend[];

    return (void *)_bss_orgend;
}
