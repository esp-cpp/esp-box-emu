#ifndef _NES_EXTERNAL_H_
#define _NES_EXTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <nes.h>
#include <nes_pal.h>
#include <nes_apu.h>
#include <mmc5_snd.h>

// External declaration of the NES context that will be defined in shared memory
extern nes_t nes;

#ifdef __cplusplus
}
#endif

#endif /* _NES_EXTERNAL_H_ */
