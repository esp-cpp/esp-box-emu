#include "libsnd_private.h"

void SsUtAllKeyOff(short mode) {
    short i;

    for (i = 0; i < _SsVmMaxVoice; i++) {
        _svm_voice[i].unk2 = 0x18;
        _svm_voice[i].unk6 = 0;
        _svm_voice[i].unke = 0xFF;
        _svm_voice[i].unk10 = 0;
        _svm_voice[i].prog = 0;
        _svm_voice[i].tone = 0xFF;
        SPUWV(i << 19 >> 19, addr, 0x200);
        SPUWV(i << 19 >> 19, pitch, 0x1000);
        SPUWV(i << 19 >> 19, adsr[0], 0x80FF);
        SPUWV(i << 19 >> 19, volume.left, 0);
        SPUWV(i << 19 >> 19, volume.right, 0);
        SPUWV(i << 19 >> 19, adsr[1], 0x4000);
        _svm_cur.voice = i;
        _SsVmKeyOffNow(1);
    }
}
