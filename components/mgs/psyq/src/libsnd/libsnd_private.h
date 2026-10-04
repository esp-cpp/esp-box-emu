// SPDX-License-Identifier: MIT
#ifndef LIBSND_INTERNAL_H
#define LIBSND_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <common.h>
#include <libsnd.h>
#include "../libspu/libspu_private.h"

#define NUM_VOICES 24
#define SEQ_FLAG_1 1
#define SEQ_FLAG_2 2
#define SEQ_FLAG_4 4
#define SEQ_FLAG_8 8
#define SEQ_FLAG_10 0x10
#define SEQ_FLAG_20 0x20
#define SEQ_FLAG_100 0x100
#define SEQ_FLAG_200 0x200
#define SEQ_FLAG_400 0x400

#define NUM_VAB 16

typedef void (*SndSsMarkCallbackProc)(short seq_no, short sep_no, short data);

#define NUM_CC 13
#define NUM_DE 20

struct Unk {
    u16 unk0;
    u16 unk2;
    u16 unk4;
    u16 unk6;
    u16 unk8;
    s16 unkA;
    s16 unkC;
    u16 unkE;
    s16 unk10;
};

struct SeqStruct {
    /* 0x00 */ u8* unk0;
    /* 0x04 */ u8* read_pos;
    /* 0x08 */ u8* next_sep_pos;
    /* 0x0C */ u8* loop_pos;
    /* 0x10 */ u8 unk10;
    /* 0x11 */ u8 unk11;
    /* 0x12 */ u8 channel;
    /* 0x13 */ u8 unk13;
    /* 0x14 */ u8 play_mode;
    /* 0x15 */ u8 unk15;
    /* 0x16 */ u8 unk16;
    /* 0x17 */ u8 unk17;
    /* 0x18 */ u8 unk18;
    /* 0x19 */ u8 unk19;
    /* 0x1A */ u8 unk1A;
    /* 0x1B */ u8 unk1B;
    /* 0x1C */ u8 unk1C;
    /* 0x1D */ u8 unk1D;
    /* 0x1E */ u8 unk1E;
    /* 0x1F */ u8 unk1F;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 unk22;
    /* 0x23 */ u8 unk23;
    /* 0x24 */ u16 unk24;
    /* 0x26 */ u8 unk26;
    /* 0x27 */ u8 panpot[16];
    /* 0x37 */ u8 programs[16];
    /* 0x47 */ u8 unk47;
    /* 0x48 */ short vol_l;
    /* 0x4A */ short vol_r;
    /* 0x4C */ s16 unk4C;
    /* 0x4E */ s16 unk4E;
    /* 0x50 */ s16 unk50;
    /* 0x52 */ s16 unk52;
    /* 0x54 */ s16 unk54;
    /* 0x56 */ s16 unk56;
    /* 0x58 */ short voll; // maybe unsigned?
    /* 0x5A */ short volr; // maybe unsigned?
    /* 0x5C */ s16 unk5C;
    /* 0x5E */ s16 unk5E;
    /* 0x60 */ short vol[16];
    /* 0x80 */ u32 unk80;
    /* 0x84 */ s32 unk84;
    /* 0x88 */ s32 delta_value;
    /* 0x8C */ s32 unk8c;
    /* 0x90 */ s32 unk90;
    /* 0x94 */ u32 unk94;
    /* 0x98 */ unsigned int flags;
    /* 0x9C */ int v_time_l;
    /* 0xA0 */ int v_time_r;
    /* 0xA4 */ u32 unkA4;
    /* 0xA8 */ s32 unkA8;
    /* 0xAC */ s32 unkAC;
};
#ifdef __psyz
STATIC_ASSERT(
    sizeof(struct SeqStruct) == SS_SEQ_TABSIZ, "SS_SEQ_TABSIZ unaligned");
#endif

struct SndSeqTickEnv {
    /* 0x00 */ s32 tick_mode;
    /* 0x04 */ s32 manual_tick;
    /* 0x08 */ void (*tick_cb)(void);
    /* 0x0C */ void (*vsync_cb)();
    /* 0x10 */ u8 vsync_tick;
    /* 0x11 */ u8 unk11;
    /* 0x12 */ u8 alarm_tick;
    /* 0x14 */ int unk14;
};

struct SpuVoice {
    s16 unk0;
    s16 unk2;
    s16 unk04;
    u16 unk6;
    s16 unk8;
    char unka;
    char unkb;
    s16 note; /* 0xC */
    s16 unke;
    s16 unk10;
    s16 prog;  /* 0x12 */
    s16 tone;  /* 0x14*/
    s16 vabId; /* 0x16 */
    s16 unk18;
    u8 pad4[1];
    u8 unk1b;
    s16 auto_vol; /* 0x1c */
    s16 unk1e;
    s16 unk20;
    s16 unk22;
    s16 start_vol; /* 0x24 */
    s16 end_vol;   /* 0x26 */
    s16 auto_pan;  /* 0x28 */
    s16 unk2a;
    s16 unk2c;
    s16 unk2e;
    s16 start_pan; /* 0x30 */
    s16 end_pan;   /* 0x32 */
};

struct struct_svm {
    char prog_tones;
    char vabId;
    char note;
    char fine;
    char volume;
    char pan;
    char prog;
    char field_7_fake_program;
    char field_8_unknown;
    char field_0x9;
    char mvol;
    char mpan;
    char tone;
    char tone_vol;
    char tone_pan;
    char tone_prior;
    char tone_center;
    unsigned char tone_shift;
    char tone_min;
    char tone_max;
    u8 tone_mode;
    u8 pad;
    /* 0x8011110E 0x16 */ short seq_sep_no;
    short tone_vag_idx;
    short voice;
    short voiceOffset;
    short field_0x1e;
};

extern s32 D_8003C74C;
extern SndSsMarkCallbackProc _SsMarkCallback[32][16];
extern s32 _snd_ev_flag;
extern s32 _snd_openflag;
extern s16 _snd_seq_s_max;
extern s16 _snd_seq_t_max;
extern struct SndSeqTickEnv _snd_seq_tick_env;
extern SPU_RXX* _svm_sreg;

/* libsnd accesses the SPU registers through its own _svm_sreg pointer, not
 * libspu's _spu_RXX. Override the SPUR/SPUW macros from libspu_private.h so the
 * decomp build relocates against _svm_sreg and matches the original library. */
#undef SPUR
#undef SPUW
#ifndef __psyz
#define SPUR(field) (_svm_sreg->field)
#define SPUW(field, val) _svm_sreg->field = (val)
/* Voice-register access via the SPU base treated as an array of voice regs.
 * Matches the original ((SPU_VOICE_REG*)_svm_sreg)[n].field codegen. */
#define SPURV(n, field) (((SPU_VOICE_REG*)_svm_sreg)[n].field)
#define SPUWV(n, field, val) ((SPU_VOICE_REG*)_svm_sreg)[n].field = (val)
#else
#define SPUR(field) Psyz_SpuRead(offsetof(SPU_RXX, field))
#define SPUW(field, val) Psyz_SpuWrite(offsetof(SPU_RXX, field), val)
#define SPURV(n, field) Psyz_SpuRead(offsetof(SPU_RXX, voice[n].field))
#define SPUWV(n, field, val)                                                   \
    Psyz_SpuWrite(offsetof(SPU_RXX, voice[n].field), val)
#endif

extern struct SeqStruct* _ss_score[32];
extern s32 _svm_brr_start_addr[];
extern short kMaxPrograms;
extern struct struct_svm _svm_cur;
extern s16 _svm_damper;
extern unsigned short _svm_okon1;
extern unsigned short _svm_okon2;
extern unsigned short _svm_okof1;
extern unsigned short _svm_okof2;
extern unsigned short _svm_orev1;
extern unsigned short _svm_orev2;
extern struct SpuVoice _svm_voice[NUM_VOICES];
extern int _svm_envx_hist[16];
extern SpuReverbAttr _svm_rattr;
extern u8 _svm_vab_used[NUM_VAB];
extern char _SsVmMaxVoice;
extern VabHdr* _svm_vab_vh[NUM_VAB];
extern ProgAtr* _svm_vab_pg[NUM_VAB];
extern VagAtr* _svm_vab_tn[NUM_VAB];
extern unsigned _svm_vab_start[NUM_VAB];
extern s32 _svm_vab_total[NUM_VAB];
extern VabHdr* _svm_vh;
extern ProgAtr* _svm_pg;
extern VagAtr* _svm_tn;
extern u_long* _svm_vg;
extern u16 _svm_vab_count;
extern u8 spuVmMaxVoice;
extern short _svm_stereo_mono;
extern u32 VBLANK_MINUS;
extern _SsFCALL SsFCALL;
extern void (*_autovol)(short voice);
extern void (*_autopan)(short voice);
extern SPU_VOICE_REG _svm_sreg_buf[NUM_VOICES];
extern char _svm_sreg_dirty[NUM_VOICES];

void SetAutoPan(short voice);
void SetAutoVol(short voice);
void SeAutoPan(short, short, short, short);
void SeAutoVol(short, short, short, short);
void Snd_SetPlayMode(s16, s16, u8, s16);
long SpuIsTransferCompleted(long);
void SpuQuit(void);
void SpuSetCommonAttr(SpuCommonAttr* attr);
s32 SpuVmGetSeqVol(s16, s16*, s16*);
void SpuVmFlush();
void SpuVmSeKeyOn(s16 arg0, s16 arg1, u16 arg2, s32 arg3, u16 arg4, u16 arg5);
s32 SpuVmSetSeqVol(s16 seq_sep_no, u16 voll, u16 volr, s16 arg3);
u32 SpuVmVSetUp(s16, s16);
void _SsInit(void);
void _SsVmInit(char numVoices);
void _SsVmFlush(void);
void _SsSeqPlay(short seq_access_num, short arg1);
void _SsSndCrescendo(short seq_access_num, short arg1);
void _SsSndDecrescendo(short seq_access_num, short arg1);
void _SsSndPause(short seq_access_num, short arg1);
void _SsSndPlay(short seq_access_num, short arg1);
void _SsSndReplay(short seq_access_num, short arg1);
void _SsSndTempo(short seq_access_num, short arg1);
void _SsSndStop(short seq_access_num, short arg1);
void _SsUtResolveADSR(u16 arg0, u16 arg1, struct Unk* arg2);
void _SsVmSeqKeyOff(s16 seq_sep_num);
void _SsVmSetSeqVol(
    short seq_sep_no, unsigned short voll, unsigned short volr, short arg3);
void _SsVmGetSeqVol(short seq_sep_no, short* voll, short* volr);
void _spu_setInTransfer(s32);
void vmNoiseOn2(u8 arg0, u16 arg1, u16 arg2, u16 arg3, u16 arg4);
void _SsSndSetVolData(
    short sep_access_num, short seq_num, short vol, int v_time);
void _SsVmDamperOff(void);
int _SsInitSoundSep(short flag, short i, short vab_id, unsigned long* addr);
char _SsVmAlloc(short voice);
void vmNoiseOn(char voice);
void vmNoiseOff(char voice);
unsigned short note2pitch2(unsigned short note, unsigned short fine);
void _SsVmKeyOnNow(unsigned short vagCount, unsigned short pitch);
int _SsVmVSetUp(short vabId, short prog);
void _SsVmDoAllocate(void);

void _SsNoteOn(short a0, short a1, unsigned char a2, unsigned char a3);
void _SsSetProgramChange(short a0, short a1, unsigned char a2);
void _SsGetMetaEvent(short a0, short a1, unsigned char a2);
void _SsSetPitchBend(short a0, short a1);
void _SsSetControlChange(short a0, short a1, unsigned char a2);
void _SsContBankChange(short a0, short a1);
void _SsContDataEntry(short a0, short a1, unsigned char a2);
void _SsContMainVol(short a0, short a1, unsigned char a2);
void _SsContPanpot(short a0, short a1, unsigned char a2);
void _SsContExpression(short a0, short a1, unsigned char a2);
void _SsContDamper(short a0, short a1, unsigned char a2);
void _SsContExternal(short a0, short a1, unsigned char a2);
void _SsContNrpn1(short a0, short a1, unsigned char a2);
void _SsContNrpn2(short a0, short a1, unsigned char a2);
void _SsContRpn1(short a0, short a1, unsigned char a2);
void _SsContRpn2(short a0, short a1, unsigned char a2);
void _SsContResetAll(short a0, short a1);

void _SsSetNrpnVabAttr0(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr1(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr2(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr3(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr4(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr5(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr6(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr7(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr8(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr9(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr10(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr11(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr12(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr13(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr14(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr15(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr16(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr17(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr18(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);
void _SsSetNrpnVabAttr19(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5);

#ifdef __cplusplus
}
#endif

#endif
