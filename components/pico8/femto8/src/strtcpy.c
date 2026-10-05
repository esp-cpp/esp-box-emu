/* This code is in the public domain.  */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "strtcpy.h"

ssize_t
strtcpy(char *restrict dst, const char *restrict src, size_t dsize)
{
    bool    trunc;
    size_t  dlen, slen;

    if (dsize == 0) {
        errno = ENOBUFS;
        return -1;
    }

    slen = strnlen(src, dsize);
    trunc = (slen == dsize);
    dlen = slen - trunc;

    memcpy(dst, src, dlen);
    dst[dlen] = '\0';
    if (trunc)
        errno = E2BIG;
    return trunc ? -1 : slen;
}