#include "libsnd_private.h"

INCLUDE_ASM("asm/nonmatchings/libsnd/ut_vvol", SsUtGetDetVVol);

short SsUtSetDetVVol(short voice, short voll, short volr) {
    volatile int dummy;
    if (voice >= 0 && voice < NUM_VOICES) {
        _svm_sreg_buf[voice].volume.right = volr;
        _svm_sreg_buf[voice].volume.left = voll;
        _svm_sreg_dirty[voice] |= 3;
        return 0;
    }
    return -1;
}

short SsUtGetVVol(short voice, short* voll, short* volr) {
    if (voice >= 0 && voice < NUM_VOICES) {
        int r = SPURV(voice, volume.right);
        *voll = SPUR(voice[voice].volume.left) / 129;
        *volr = r / 129;
        return 0;
    }
    return -1;
}

#ifndef __psyz
INCLUDE_ASM("asm/nonmatchings/libsnd/ut_vvol", SsUtSetVVol);
#else
short SsUtSetVVol(short voice, short voll, short volr) {
    volatile int dummy;
    unsigned short l;
    unsigned short r;
    if (voice >= 0 && voice < NUM_VOICES) {
        l = voll * 0x81;
        r = volr * 0x81;
        _svm_sreg_buf[voice].volume.right = (r * r) / 16383;
        _svm_sreg_buf[voice].volume.left = (l * l) / 16383;
        _svm_sreg_dirty[voice] |= 3;
        return 0;
    }
    return -1;
}
#endif
