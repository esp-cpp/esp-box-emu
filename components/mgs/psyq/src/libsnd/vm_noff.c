#include "libsnd_private.h"

void vmNoiseOff(char voice) {
    _svm_voice[voice].unk1b = 0;
    _svm_voice[voice].unk04 = 0;
    SPUW(noise_mode[0], 0);
    SPUW(noise_mode[1], 0);
}
