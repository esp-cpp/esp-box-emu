#include "libsnd_private.h"

char SsSetReservedVoice(char voices) {
    if (voices > NUM_VOICES || voices == 0) {
        return -1;
    }
    _SsVmMaxVoice = voices;
    return _SsVmMaxVoice;
}
