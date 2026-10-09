#ifndef GTEMAC_SHIM_H
#define GTEMAC_SHIM_H
#include <libgte.h>
#include <stdlib.h>
/* PSY-Q's rand() returns 0..0x7fff and the game scales its results by 15
 * bits; the hosted libc's is 31-bit. Every C file of the component sees this
 * (the header is force-included); psyz_port.c undefines it to reach libc. */
int psyz_rand(void);
#define rand psyz_rand
/* PSX 1KiB scratchpad at 0x1F800000: a plain array on any other target */
extern unsigned long psyz_scratchpad[256];
#define getScratchAddr(n) ((unsigned long*)&psyz_scratchpad[(n)])
/* PsyQ inline forms that differ from psyz's plain-C prototypes */
#define gte_NormalClip(a, b, c, out) (*(int*)(out) = NormalClip((a), (b), (c)))
MATRIX* psyz_CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2);
#define gte_CompMatrix(a, b, c) psyz_CompMatrix((MATRIX*)(a), (MATRIX*)(b), (MATRIX*)(c))
VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1);
#endif
