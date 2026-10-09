#include "libspu_private.h"

inline long SpuRGetAllKeysStatus(long min, long max, char* status) {
    volatile SPU_VOICE_REG* voices;
    s32 voice;
    int voice_mask;
    u16 volumex;

    if (min < 0) {
        min = 0;
    }
    if (min >= 24) {
        return -3;
    }
    if (max >= 24) {
        max = 24 - 1;
    }
    if (max < 0 || max < min) {
        return -3;
    }

    max++;
    for (voice = min; voice < max; voice++) {
#ifndef __psyz
        voices = _spu_RXX->rxx.voice;
        volumex = voices[voice].volumex;
#else
        volumex = SPUR(voice[voice].volumex);
#endif
        voice_mask = 1 << voice;
        if (_spu_keystat & voice_mask) {
            if (volumex > 0) {
                status[voice] = 1;
            } else {
                status[voice] = 3;
            }
        } else if (volumex > 0) {
            status[voice] = 2;
        } else {
            status[voice] = 0;
        }
    }

    return 0;
}

void SpuGetAllKeysStatus(char* status) {
    SpuRGetAllKeysStatus(0, NUM_VOICES, status);
}
