#include "libspu_private.h"

#ifndef __psyz
INCLUDE_ASM("asm/nonmatchings/libspu/s_sk", SpuSetKey);
#else
typedef struct {
    u16 unk0; // key_on
    u16 unk2; // key_on
    u16 unk4; // key off
    u16 unk6; // key off
} SpuRQ;
extern SpuRQ _spu_RQ;

void SpuSetKey(long on_off, u_long voice_bit) {
    u32 temp_a1;
    unsigned short new_var;
    u32 temp_a3;
    temp_a1 = voice_bit & 0xFFFFFF;
    temp_a3 = temp_a1 >> 0x10;

    switch (on_off) {
    case 1:
        if (_spu_env & 1) {
            _spu_RQ.unk0 = temp_a1;
            _spu_RQ.unk2 = temp_a3;
            _spu_RQmask |= 1;
            _spu_RQvoice |= temp_a1;
            if (_spu_RQ.unk4 & temp_a1) {
                _spu_RQ.unk4 &= ~temp_a1;
            }
            if (_spu_RQ.unk6 & temp_a3) {
                _spu_RQ.unk4 &= ~temp_a3;
            }
        } else {
            SPUW(key_on[0], temp_a1);
            SPUW(key_on[1], temp_a3);
            _spu_keystat |= temp_a1;
        }
        break;
    case 0:
        if (_spu_env & 1) {
            _spu_RQ.unk4 = temp_a1;
            _spu_RQ.unk6 = temp_a3;
            _spu_RQmask |= 1;
            _spu_RQvoice &= ~temp_a1;
            if (_spu_RQ.unk0 & temp_a1) {
                _spu_RQ.unk0 &= ~temp_a1;
            }
            if (_spu_RQ.unk2 & temp_a3) {
                _spu_RQ.unk2 &= ~temp_a3;
            }
        } else {
            SPUW(key_off[0], temp_a1);
            SPUW(key_off[1], temp_a3);
            _spu_keystat &= ~temp_a1;
        }
        break;
    }
}
#endif
