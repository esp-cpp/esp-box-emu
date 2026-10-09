#include "libsnd_private.h"

void SsSetMVol(short voll, short volr) {
    SpuCommonAttr attr;

    attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    attr.mvol.left = voll * 129;
    attr.mvol.right = volr * 129;
    SpuSetCommonAttr(&attr);
}
