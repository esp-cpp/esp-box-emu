#include <kernel.h>
#include <libapi.h>
#include <libetc.h>
#include "libspu_private.h"

static int D_800D0B80[] = {0x15117350, 0x0040B49C};
s32 _spu_EVdma = 0;
s32 _spu_keystat = 0;
int _spu_trans_mode = 0;
s32 _spu_rev_flag = 0;
s32 _spu_rev_reserve_wa = 0;
s32 _spu_rev_offsetaddr = 0;
SpuReverbAttr _spu_rev_attr = {0};
s32 _spu_RQvoice = 0;
s32 _spu_RQmask = 0;
unsigned short _spu_voice_centerNote[] = {
    0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
    0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
    0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
};
int _spu_zerobuf[256] = {};
s32 _spu_env = 0;
s32 _spu_isCalled = 0;

void _SpuInit(int bHot) {
    int i;

    ResetCallback();
    _spu_init(bHot);

    if (bHot == 0) {
        for (i = 0; i < NUM_VOICES; i++) {
            _spu_voice_centerNote[i] = 0xC000;
        }
    }

    SpuStart();
    _spu_rev_flag = 0;
    _spu_rev_reserve_wa = 0;
    _spu_rev_attr.mode = 0;
    _spu_rev_attr.depth.left = 0;
    _spu_rev_attr.depth.right = 0;
    _spu_rev_attr.delay = 0;
    _spu_rev_attr.feedback = 0;
    _spu_rev_offsetaddr = _spu_rev_startaddr[0];
    _spu_FsetRXX(SPU_RXX_REV_WA_START_ADDR, _spu_rev_offsetaddr, 0);
    _spu_AllocBlockNum = 0;
    _spu_AllocLastNum = 0;
    _spu_memList = 0;
    _spu_trans_mode = 0;
    _spu_transMode = 0;
    _spu_keystat = 0;
    _spu_RQmask = 0;
    _spu_RQvoice = 0;
    _spu_env = 0;
}

void SpuStart(void) {
    if (_spu_isCalled == 0) {
        _spu_isCalled = 1;
        EnterCriticalSection();
        _SpuDataCallback(_spu_FiDMA);
        _spu_EVdma = OpenEvent(HwSPU, EvSpCOMP, EvMdNOINTR, NULL);
        EnableEvent(_spu_EVdma);
        ExitCriticalSection();
    }
}
