#include "libsnd_private.h"

short SsUtChangeADSR(short voice, short vabId, short prog, short old_note,
                     unsigned short adsr1, unsigned short adsr2) {
    volatile int dummy;
    if (voice >= 0 && voice < NUM_VOICES) {
        if (_svm_voice[voice].vabId == vabId &&
            _svm_voice[voice].prog == prog &&
            _svm_voice[voice].note == old_note) {
            _svm_sreg_buf[voice].adsr[0] = adsr1;
            _svm_sreg_buf[voice].adsr[1] = adsr2;
            _svm_sreg_dirty[voice] |= 0x30;
            return 0;
        }
    }
    return -1;
}
