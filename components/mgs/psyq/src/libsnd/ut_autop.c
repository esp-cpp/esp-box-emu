#include "libsnd_private.h"

short SsUtAutoPan(
    short voice, short start_pan, short end_pan, short delta_time) {
    _autopan = SetAutoPan;
    if (voice >= 0 && voice < NUM_VOICES) {
        SeAutoPan(voice, start_pan, end_pan, delta_time);
        return 0;
    }
    return -1;
}
