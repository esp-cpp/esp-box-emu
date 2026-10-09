#include "libsnd_private.h"

SPU_RXX* _svm_sreg = (SPU_RXX*)0x1F801C00;
SPU_VOICE_REG _svm_sreg_buf[NUM_VOICES];
char _svm_sreg_dirty[NUM_VOICES];
unsigned short _svm_okon1;
unsigned short _svm_okon2;
unsigned short _svm_okof1;
unsigned short _svm_okof2;
unsigned short _svm_orev1;
unsigned short _svm_orev2;
struct SpuVoice _svm_voice[NUM_VOICES];
unsigned int _svm_envx_ptr;
static int _dummy;
int _svm_envx_hist[16];
void (*_autovol)(short voice);
void (*_autopan)(short voice);

extern unsigned char _svm_auto_kof_mode;

#ifndef __psyz
INCLUDE_ASM("asm/nonmatchings/libsnd/vm_f", _SsVmFlush);
#else
void _SsVmFlush(void) {
    s32 i;
    unsigned int var_s2;
    u16 temp_a0;
    u16 temp_a1;
    u16 temp_a2;
    u16 temp_v1;

    _svm_envx_ptr = (_svm_envx_ptr + 1) & 0xF;
    _svm_envx_hist[_svm_envx_ptr] = 0;
    for (i = 0; i < _SsVmMaxVoice; i++) {
        _svm_voice[i].unk6 = SPUR(voice[i].volumex);
        if (!_svm_voice[i].unk6) {
            _svm_envx_hist[_svm_envx_ptr] |= 1 << i;
        }
    }
    if (_svm_auto_kof_mode == 0) {
        var_s2 = -1;
        for (i = 0; i < LEN(_svm_envx_hist) - 1; i++) {
            var_s2 &= _svm_envx_hist[i];
        }
        for (i = 0; i < _SsVmMaxVoice; i++) {
            if (var_s2 & (1 << i)) {
                if (_svm_voice[i].unk1b == 2) {
                    SpuSetNoiseVoice(0, 0xFFFFFF);
                }
                _svm_voice[i].unk1b = 0;
            }
        }
    }
    _svm_okon1 &= ~_svm_okof1;
    _svm_okon2 &= ~_svm_okof2;
    for (i = 0; i < NUM_VOICES; i++) {
        if (_svm_voice[i].auto_vol) {
            _autovol(i);
        }
        if (_svm_voice[i].auto_pan) {
            _autopan(i);
        }
    }
    for (i = 0; i < NUM_VOICES; i++) {
        if (_svm_sreg_dirty[i] & 1) {
            SPUW(voice[i].volume.left, _svm_sreg_buf[i].volume.left);
            SPUW(voice[i].volume.right, _svm_sreg_buf[i].volume.right);
        }
        if (_svm_sreg_dirty[i] & 4) {
            SPUW(voice[i].pitch, _svm_sreg_buf[i].pitch);
        }
        if (_svm_sreg_dirty[i] & 8) {
            SPUW(voice[i].addr, _svm_sreg_buf[i].addr);
        }
        if (_svm_sreg_dirty[i] & 0x10) {
            SPUW(voice[i].adsr[0], _svm_sreg_buf[i].adsr[0]);
            SPUW(voice[i].adsr[1], _svm_sreg_buf[i].adsr[1]);
        }
        _svm_sreg_dirty[i] = 0;
    }
    temp_v1 = _svm_okof1;
    temp_a0 = _svm_okof2;
    temp_a1 = _svm_okon1;
    temp_a2 = _svm_okon2;
    _svm_okof1 = 0;
    _svm_okof2 = 0;
    _svm_okon1 = 0;
    _svm_okon2 = 0;
    SPUW(key_off[0], temp_v1);
    SPUW(key_off[1], temp_a0);
    SPUW(key_on[0], temp_a1);
    SPUW(key_on[1], temp_a2);
    SPUW(rev_mode[0], _svm_orev1);
    SPUW(rev_mode[1], _svm_orev2);
}
#endif
