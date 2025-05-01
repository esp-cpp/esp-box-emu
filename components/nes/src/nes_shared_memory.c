#include "nes_shared_memory.h"
#include "nes_external.h"
#include "shared_memory.h"

#include <string.h>

// Define the external nes variable in shared memory
nes_t *nes_context;

/* allocates memory and clears it */
void *_my_malloc(int size) {
    return shared_malloc(size);
}

/* free a pointer allocated with my_malloc */
void _my_free(void **data) {
}

extern int32 *decay_lut; // 16
extern int *vbl_lut; // [32];
extern int *trilength_lut; // [128];

/* noise lookups for both modes */
#ifndef REALTIME_NOISE
extern int8 *noise_long_lut; // [APU_NOISE_32K];
extern int8 *noise_short_lut; // [APU_NOISE_93];
#endif /* !REALTIME_NOISE */

extern int32 *mmc5_decay_lut; // [16];
extern int *mmc5_vbl_lut; // [32];
extern mmc5_t *mmc5;

void nes_init_shared_memory(void) {
    nes_palette = (rgb_t*)shared_malloc(sizeof(rgb_t)* 256);

    decay_lut = (int32*)shared_malloc(sizeof(int32)*16); // 16
    vbl_lut = (int*)shared_malloc(sizeof(int)*32); // [32];
    trilength_lut = (int*)shared_malloc(sizeof(int)*128); // [128];

    /* noise lookups for both modes */
#ifndef REALTIME_NOISE
    noise_long_lut = (int8*)shared_malloc(sizeof(int8)*APU_NOISE_32K); // [APU_NOISE_32K];
    noise_short_lut = (int8*)shared_malloc(sizeof(int8)*APU_NOISE_93); // [APU_NOISE_93];
#endif /* !REALTIME_NOISE */

    mmc5_decay_lut = (int32 *)shared_malloc(sizeof(int32) * 16); // [16];
    mmc5_vbl_lut = (int *)shared_malloc(sizeof(int) * 32); // [32];

    mmc5 = (mmc5_t*)shared_malloc(sizeof(mmc5_t));

    nes_cpu = (nes6502_context *)_my_malloc(sizeof(nes6502_context));
}

void nes_free_shared_memory(void) {
    shared_mem_clear();
}
