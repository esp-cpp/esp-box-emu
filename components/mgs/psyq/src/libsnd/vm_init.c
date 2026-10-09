#include "libsnd_private.h"

extern SPU_RXX* _svm_sreg;
extern SPU_VOICE_REG _svm_sreg_buf[NUM_VOICES];
extern char _svm_sreg_dirty[NUM_VOICES];
extern unsigned short _svm_okon1;
extern unsigned short _svm_okon2;
extern unsigned short _svm_okof1;
extern unsigned short _svm_okof2;
extern unsigned short _svm_orev1;
extern unsigned short _svm_orev2;
extern struct SpuVoice _svm_voice[NUM_VOICES];
extern char _svm_auto_kof_mode;

char _ss_spu_vm_rec[0x110];

#ifndef __psyz
INCLUDE_ASM("asm/nonmatchings/libsnd/vm_init", _SsVmInit);
#else
void _SsVmInit(char numVoices) {
    unsigned short i;

    _spu_setInTransfer(0);
    _svm_damper = 0;
    SpuInitMalloc(32, _ss_spu_vm_rec + 8);
    // for (i = 0; i < LEN(_svm_sreg_buf); i++) {
    //     _svm_sreg_buf[i] = 0;
    // }
    memset(_svm_sreg_buf, 0, sizeof(_svm_sreg_buf)); // not matching
    for (i = 0; i < NUM_VOICES; i++) {
        _svm_sreg_dirty[i] = 0;
    }
    _svm_vab_count = 0;
    for (i = 0; i < NUM_VAB; i++) {
        _svm_vab_used[i] = 0;
    }

    if (numVoices >= NUM_VOICES) {
        _SsVmMaxVoice = NUM_VOICES;
    } else {
        _SsVmMaxVoice = numVoices;
    }

    for (i = 0; i < _SsVmMaxVoice; i++) {
        _svm_voice[i].unk2 = 0x18;
        _svm_voice[i].unk0 = 0xFF;
        _svm_voice[i].unke = -1;
        _svm_voice[i].unk1b = 0;
        _svm_voice[i].unk04 = 0;
        _svm_voice[i].unk6 = 0;
        _svm_voice[i].unk10 = 0;
        _svm_voice[i].prog = 0;
        _svm_voice[i].tone = 0xFF;
        _svm_voice[i].unk8 = 0;
        _svm_voice[i].unka = 0x40;
        _svm_voice[i].auto_vol = 0;
        _svm_voice[i].unk1e = 0;
        _svm_voice[i].unk20 = 0;
        _svm_voice[i].unk22 = 0;
        _svm_voice[i].auto_pan = 0;
        _svm_voice[i].unk2a = 0;
        _svm_voice[i].unk2c = 0;
        _svm_voice[i].unk2e = 0;
        _svm_voice[i].start_pan = 0;
        _svm_voice[i].start_vol = 0;
        SPUW(voice[i].addr, 0x200);
        SPUW(voice[i].pitch, 0x1000);
        SPUW(voice[i].adsr[0], 0x80FF);
        SPUW(voice[i].volume.left, 0);
        SPUW(voice[i].volume.right, 0);
        SPUW(voice[i].adsr[1], 0x4000);
        _svm_cur.voice = i;
        _SsVmKeyOffNow(1);
    }
    _svm_rattr.depth.left = 0x3FFF;
    _svm_rattr.depth.right = 0x3FFF;
    _svm_okon1 = 0;
    _svm_okon2 = 0;
    _svm_okof1 = 0;
    _svm_orev1 = 0;
    _svm_orev2 = 0;
    _svm_rattr.mask = 0;
    _svm_rattr.mode = 0;
    _svm_auto_kof_mode = 0;
    _svm_stereo_mono = 0;
    kMaxPrograms = 128;
    _SsVmFlush();
}
#endif