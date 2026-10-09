#include <psyz.h>
#include <libspu.h>
#include <libsnd.h>
#include <psyz/log.h>
#include "libspu/libspu_private.h" /* esp-box-emu: psyq/src on the include path */

#define LEN(x) ((s32)(sizeof(x) / sizeof(*(x))))
#define NUM_VOICES 24

typedef void (*SndSsMarkCallbackProc)(short seq_no, short sep_no, short data);

extern short _snd_seq_s_max;
extern short _snd_seq_t_max;
extern int _snd_ev_flag;
extern _SsFCALL SsFCALL;
extern SndSsMarkCallbackProc _SsMarkCallback[32][16];
extern struct SeqStruct* _ss_score[32];
extern unsigned int VBLANK_MINUS;
extern int _snd_openflag;

static void SetVoiceData(int nVoice, unsigned short* data) {
    for (int i = 0; i < 8; i++) {
        Psyz_SpuWrite(nVoice * 0x10 + i * 2, data[i]);
    }
}

static void SetStateData(unsigned short* data, unsigned nWords) {
    for (unsigned i = 0; i < nWords; i++) {
        Psyz_SpuWrite(0x180 + i * 2, data[i]);
    }
}

static unsigned short default_voice[] = {0, 0, 0x1000, 0x3000, 0x00BF, 0, 0, 0};
static unsigned short default_state[] = {
    0x3FFF, 0x3FFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
extern SPU_RXX* _svm_sreg;
void _SsVmInit(int num_voices);
void _SsInit(void) {
    int i, j;

    _svm_sreg = (SPU_RXX*)_spu_RXX;
    for (i = 0; i < NUM_VOICES; i++) {
        SetVoiceData(i, default_voice);
    }
    SetStateData(default_state, LEN(default_state));

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

// void _SsVmFlush(void) { NOT_IMPLEMENTED; }

void _SsSeqPlay(short arg0, short arg1) { NOT_IMPLEMENTED; }

void _SsSndTempo(short arg0, short arg1) { NOT_IMPLEMENTED; }

short SsSeqOpen(u_long* addr, short vab_id) {
    NOT_IMPLEMENTED;
    return 0;
}

void SsSetMarkCallback(
    short access_num, short seq_num, SsMarkCallbackProc proc) {
    NOT_IMPLEMENTED;
}

void SsSeqSetDecrescendo(short seq_access_num, short vol, long v_time) {
    NOT_IMPLEMENTED;
}

void _SsVmGetSeqVol(short seq_sep_no, short* voll, short* volr) {
    NOT_IMPLEMENTED;
}

int _SsInitSoundSep(short flag, short i, short vab_id, unsigned int* addr) {
    NOT_IMPLEMENTED;
    return -1;
}

char _SsVmAlloc(short voice) {
    NOT_IMPLEMENTED;
    return -1;
}

void _SsVmSetSeqVol(
    short seq_sep_no, unsigned short voll, unsigned short volr, short arg3) {
    NOT_IMPLEMENTED;
}

void _SsVmSeqKeyOff(s16 seq_sep_num) { NOT_IMPLEMENTED; }

void vmNoiseOn(char voice) { NOT_IMPLEMENTED; }

void SetAutoVol(int voices) { NOT_IMPLEMENTED; }

void SeAutoPan(short arg0, short arg1, short arg2, short arg3) {
    NOT_IMPLEMENTED;
}

void _SsNoteOn(short a0, short a1, unsigned char a2, unsigned char a3) {
    NOT_IMPLEMENTED;
}
void _SsSetProgramChange(short a0, short a1, unsigned char a2) {
    NOT_IMPLEMENTED;
}
void _SsGetMetaEvent(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsSetPitchBend(short a0, short a1) { NOT_IMPLEMENTED; }
void _SsSetControlChange(short a0, short a1, unsigned char a2) {
    NOT_IMPLEMENTED;
}
void _SsContBankChange(short a0, short a1) { NOT_IMPLEMENTED; }
void _SsContDataEntry(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContMainVol(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContPanpot(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContExpression(short a0, short a1, unsigned char a2) {
    NOT_IMPLEMENTED;
}
void _SsContDamper(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContExternal(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContNrpn1(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContNrpn2(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContRpn1(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContRpn2(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContResetAll(short a0, short a1) { NOT_IMPLEMENTED; }

void _SsSetNrpnVabAttr0(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr1(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr2(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr3(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr4(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr5(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr6(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr7(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr8(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr9(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr10(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr11(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr12(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr13(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr14(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr15(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr16(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr17(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr18(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr19(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
