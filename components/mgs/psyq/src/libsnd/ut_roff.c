#include <common.h>
#include <libsnd.h>

s32 SpuSetReverb(s32);

void SsUtReverbOff(void) { SpuSetReverb(0); }
