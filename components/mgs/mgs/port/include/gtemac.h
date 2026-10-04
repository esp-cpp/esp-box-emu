#ifndef GTEMAC_SHIM_H
#define GTEMAC_SHIM_H
#include <libgte.h>
/* PSX 1KiB scratchpad at 0x1F800000: a plain array on any other target */
extern unsigned long psyz_scratchpad[256];
#define getScratchAddr(n) ((unsigned long*)&psyz_scratchpad[(n)])
/* PsyQ inline forms that differ from psyz's plain-C prototypes */
#define gte_NormalClip(a, b, c, out) (*(int*)(out) = NormalClip((a), (b), (c)))
MATRIX* psyz_CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2);
#define gte_CompMatrix(a, b, c) psyz_CompMatrix((MATRIX*)(a), (MATRIX*)(b), (MATRIX*)(c))
VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1);
#endif
