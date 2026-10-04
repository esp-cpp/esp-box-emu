#include "libspu_private.h"

void WaitEvent(s32);
extern s32 _SpuIsInAllocateArea_(u32 addr);
extern s32 _spu_zerobuf[];

long SpuClearReverbWorkArea(long rev_mode) {
    void (* volatile callback)();
    s32 oldTransmode;
    s32 dstAddr;
    s32 var_s3;
    s32 transmodeCleared;
    unsigned size;
    unsigned length;

    callback = 0;
    transmodeCleared = 0;
    if (rev_mode < 0 || rev_mode >= SPU_REV_MODE_MAX ||
        _SpuIsInAllocateArea_(_spu_rev_startaddr[rev_mode])) {
        return -1;
    }
    if (rev_mode == SPU_REV_MODE_OFF) {
        length = 0x10 << _spu_mem_mode_plus;
        dstAddr = 0xFFF0 << _spu_mem_mode_plus;
    } else {
        length = (0x10000 - _spu_rev_startaddr[rev_mode]) << _spu_mem_mode_plus;
        dstAddr = _spu_rev_startaddr[rev_mode] << _spu_mem_mode_plus;
    }
    oldTransmode = _spu_transMode;
    if (_spu_transMode == 1) {
        _spu_transMode = 0;
        transmodeCleared = 1;
    }
    var_s3 = 1;
    if (_spu_transferCallback != 0) {
        callback = _spu_transferCallback;
        _spu_transferCallback = 0;
    }
    while (var_s3 != 0) {
        size = length;
        if (length > 0x400) {
            size = 0x400;
        } else {
            var_s3 = 0;
        }
#ifdef __psyz
        SPUW(trans_addr, dstAddr >> _spu_mem_mode_plus);
        Psyz_SpuMemWrite(
            SPUR(trans_addr) << _spu_mem_mode_plus, _spu_zerobuf, size);
#else
        _spu_t(2, dstAddr);
        _spu_t(1);
        _spu_t(3, &_spu_zerobuf[0], size);
#endif
        WaitEvent(_spu_EVdma);
        length -= 0x400;
        dstAddr += 0x400;
    }
    if (transmodeCleared != 0) {
        _spu_transMode = oldTransmode;
    }
    if (callback != 0) {
        _spu_transferCallback = callback;
    }
    return 0;
}
