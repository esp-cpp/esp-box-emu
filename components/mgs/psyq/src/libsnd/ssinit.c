#include "libsnd_private.h"

s16 _snd_seq_s_max;
s16 _snd_seq_t_max;
s32 _snd_ev_flag;
_SsFCALL SsFCALL;
SndSsMarkCallbackProc _SsMarkCallback[32][16];
struct SeqStruct* _ss_score[32];
u32 VBLANK_MINUS;
s32 _snd_openflag;

static int D_800D0998[] = {0x15107350, 0x0040B19C};

// matches SndRegisterAttr struct
static s16 default_voice[] = {0, 0, 0x1000, 0x3000, 0x00BF, 0, 0, 0};

static s16 default_state[] = {
    0x3FFF, 0x3FFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

#ifndef __psyz
void _SsInit(void) {
    short* ptr;
    int i, j;

    ptr = (short*)0x1F801C00;
    for (i = 0; i < NUM_VOICES; i++) {
        for (j = 0; j < LEN(default_voice); j++) {
            *ptr++ = default_voice[j];
        }
    }

    ptr = (short*)0x1F801D80;
    for (i = 0; i < LEN(default_state); i++) {
        *ptr++ = default_state[i];
    }

    _SsVmInit(NUM_VOICES);
    for (j = 0; j < LEN(_SsMarkCallback); j++) {
        for (i = 0; i < LEN(*_SsMarkCallback); i++) {
            _SsMarkCallback[j][i] = NULL;
        }
    }

    VBLANK_MINUS = 60;
    _snd_openflag = 0;
    _snd_ev_flag = 0;
}
#endif
