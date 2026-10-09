#include "libspu_private.h"

u_long SpuSetNoiseVoice(long on_off, u_long voice_bit) {
    return _SpuSetAnyVoice(on_off, voice_bit, 0xCA, 0xCB);
}
