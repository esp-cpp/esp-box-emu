#include "libsnd_private.h"

void SsSetSerialVol(char s_num, short voll, short volr) {
    SpuCommonAttr attr;

    if (s_num == 0) {
        attr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
        if (voll > 0x7F) {
            voll = 0x7F;
        }
        if (volr > 0x7F) {
            volr = 0x7F;
        }
        attr.cd.volume.left = voll * 258;
        attr.cd.volume.right = volr * 258;
    }
    if (s_num == 1) {
        attr.mask = SPU_COMMON_EXTVOLL | SPU_COMMON_EXTVOLR;
        if (voll > 0x7F) {
            voll = 0x7F;
        }
        if (volr > 0x7F) {
            volr = 0x7F;
        }
        attr.ext.volume.left = voll * 258;
        attr.ext.volume.right = volr * 258;
    }
    SpuSetCommonAttr(&attr);
}
