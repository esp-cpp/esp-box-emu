#include "libsnd_private.h"

void SsSetSerialAttr(char s_num, char attr, char mode) {
    SpuCommonAttr param;

    if (s_num == 0 && attr == 0) {
        param.mask = SPU_COMMON_CDMIX;
        param.cd.mix = mode;
    }
    if (s_num == 0 && attr == 1) {
        param.mask = SPU_COMMON_CDREV;
        param.cd.reverb = mode;
    }
    if (s_num == 1 && attr == 0) {
        param.mask = SPU_COMMON_EXTMIX;
        param.ext.mix = mode;
    }
    if (s_num == 1 && attr == 1) {
        param.mask = SPU_COMMON_EXTREV;
        param.ext.reverb = mode;
    }
    SpuSetCommonAttr(&param);
}
