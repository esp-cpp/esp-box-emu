#include "libsnd_private.h"

#ifndef __psyz
INCLUDE_ASM("asm/nonmatchings/libsnd/vm_aloc2", _SsVmDoAllocate);
#else
void _SsVmDoAllocate(void) {
    volatile char pad;
    int i;
    int progIdx;
    int voice;

    _svm_cur.voiceOffset = _svm_cur.voice * sizeof(SPU_VOICE_REG) / 2;
    _svm_cur.field_0x1e = _svm_cur.field_7_fake_program * 16 + _svm_cur.tone;
    _svm_voice[_svm_cur.voice].unk6 = 0x7FFF;
    for (i = 0; i < NUM_VAB; i++) {
        _svm_envx_hist[i] &= ~(1 << _svm_cur.voice);
    }
    if ((_svm_cur.tone_vag_idx & 1) > 0) {
        progIdx = (_svm_cur.tone_vag_idx - 1) / 2;
        ((short*)_svm_sreg_buf)[_svm_cur.voiceOffset + 3] =
            ((u16*)&_svm_pg[progIdx].reserved2)[0];
    } else {
        progIdx = (_svm_cur.tone_vag_idx - 1) / 2;
        ((short*)_svm_sreg_buf)[_svm_cur.voiceOffset + 3] =
            ((u16*)&_svm_pg[progIdx].reserved2)[1];
    }
    voice = _svm_cur.voice; // FAKE!
    _svm_sreg_dirty[_svm_cur.voice] |= 8;
    _svm_sreg_buf[_svm_cur.voice].adsr[0] =
        _svm_tn[_svm_cur.field_7_fake_program * 16 + _svm_cur.tone].adsr1;
    _svm_sreg_buf[_svm_cur.voice].adsr[1] =
        _svm_tn[_svm_cur.field_7_fake_program * 16 + _svm_cur.tone].adsr2 +
        _svm_damper;
    _svm_sreg_dirty[_svm_cur.voice] |= 0x30;
}
#endif
