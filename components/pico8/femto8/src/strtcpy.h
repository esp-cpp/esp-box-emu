/* This code is in the public domain.  */

#ifndef STRTCPY_H
#define STRTCPY_H

#include <stddef.h>
#include <sys/types.h>

/*
 * strtcpy - Copy a string into a sized buffer, always NUL-terminating.
 *
 * Copies at most dsize-1 characters from src into dst, always appends '\0'.
 *
 * Returns:
 *   >= 0  number of characters written (excluding NUL) when src fits
 *   -1    if dsize == 0 (errno = ENOBUFS) or src was truncated (errno = E2BIG)
 */
ssize_t strtcpy(char *restrict dst, const char *restrict src, size_t dsize);

#endif /* STRTCPY_H */
