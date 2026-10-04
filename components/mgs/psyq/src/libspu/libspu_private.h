// SPDX-License-Identifier: MIT
#ifndef LIBSPU_INTERNAL_H
#define LIBSPU_INTERNAL_H

#include <common.h>
#include <libspu.h>
#include "libdma.h"

#define NUM_VOICES 24

#define SPU_VOL_MODE_MASK (1 << 15)
#define SPU_VOL_MAX 0x7F

// SPU Control Register (SPUCNT) bit masks
#define SPU_CTRL_MASK_CD_AUDIO_ENABLE (1 << 0)
#define SPU_CTRL_MASK_EXT_AUDIO_ENABLE (1 << 1)
#define SPU_CTRL_MASK_CD_AUDIO_REVERB (1 << 2)
#define SPU_CTRL_MASK_EXT_AUDIO_REVERB (1 << 3)
#define SPU_CTRL_MASK_TRANSFER_MANUAL_WRITE (1 << 4)
#define SPU_CTRL_MASK_TRANSFER_DMA_WRITE (2 << 4)
#define SPU_CTRL_MASK_TRANSFER_DMA_READ (3 << 4)
#define SPU_CTRL_MASK_SRAM_TRANSFER_MODE ((1 << 4) | (1 << 5))
#define SPU_CTRL_MASK_IRQ9_ENABLE (1 << 6)
#define SPU_CTRL_MASK_REVERB_MASTER_ENABLE (1 << 7)
#define SPU_CTRL_MASK_NOISE_FREQ_STEP ((1 << 8) | (1 << 9))
#define SPU_CTRL_MASK_NOISE_FREQ_SHIFT                                         \
    ((1 << 10) | (1 << 11) | (1 << 12) | (1 << 13))
#define SPU_CTRL_MASK_MUTE_SPU (1 << 14)
#define SPU_CTRL_MASK_SPU_ENABLE (1 << 15)

// SPU Register volume modes
#define SPU_VOL_MODE_DIRECT 0x0000
#define SPU_VOL_MODE_LINEARIncN 0x8000
#define SPU_VOL_MODE_LINEARIncR 0x9000
#define SPU_VOL_MODE_LINEARDecN 0xA000
#define SPU_VOL_MODE_LINEARDecR 0xB000
#define SPU_VOL_MODE_EXPIncN 0xC000
#define SPU_VOL_MODE_EXPIncR 0xD000
#define SPU_VOL_MODE_EXPDec 0xE000

#define SPU_RXX_REV_WA_START_ADDR 0xD1 /**< Reverb work area start address */

struct rev_param_entry {
    u32 flags;
    u16 dAPF1;
    u16 dAPF2;
    u16 vIIR;
    u16 vCOMB1;
    u16 vCOMB2;
    u16 vCOMB3;
    u16 vCOMB4;
    u16 vWALL;
    u16 vAPF1;
    u16 vAPF2;
    u16 mLSAME;
    u16 mRSAME;
    u16 mLCOMB1;
    u16 mRCOMB1;
    u16 mLCOMB2;
    u16 mRCOMB2;
    u16 dLSAME;
    u16 dRSAME;
    u16 mLDIFF;
    u16 mRDIFF;
    u16 mLCOMB3;
    u16 mRCOMB3;
    u16 mLCOMB4;
    u16 mRCOMB4;
    u16 dLDIFF;
    u16 dRDIFF;
    u16 mLAPF1;
    u16 mRAPF1;
    u16 mLAPF2;
    u16 mRAPF2;
    u16 vLIN;
    u16 vRIN;
};

typedef struct tagSpuMalloc {
    unsigned addr; // SPU RAM address with flag bits (always fits in 32 bits)
    unsigned size; // Block size in bytes (max 512KB)
} SPU_MALLOC;

typedef struct tagSpuVoiceRegister {
    /* 0x00 */ SpuVolume volume;
    /* 0x04 */ u16 pitch;
    /* 0x06 */ u16 addr;
    /* 0x08 */ u16 adsr[2];
    /* 0x0C */ u16 volumex;
    /* 0x0E */ u16 loop_addr;
} SPU_VOICE_REG; // size:0x10

typedef struct tagSpuControl {
    /* 0x1F801C00 0x000 */ SPU_VOICE_REG voice[NUM_VOICES];
    /* 0x1F801D80 0x180 */ SpuVolume main_vol;
    /* 0x1F801D84 0x184 */ SpuVolume rev_vol;
    /* 0x1F801D88 0x188 */ u16 key_on[2];
    /* 0x1F801D8C 0x18C */ u16 key_off[2];
    /* 0x1F801D90 0x190 */ u16 chan_fm[2];
    /* 0x1F801D94 0x194 */ u16 noise_mode[2];
    /* 0x1F801D98 0x198 */ u16 rev_mode[2];
    /* 0x1F801D9C 0x19C */ u32 chan_on;
    /* 0x1F801DA0 0x1A0 */ u16 unk;
    /* 0x1F801DA2 0x1A2 */ u16 rev_work_addr;
    /* 0x1F801DA4 0x1A4 */ u16 irq_addr;
    /* 0x1F801DA6 0x1A6 */ u16 trans_addr;
    /* 0x1F801DA8 0x1A8 */ u16 trans_fifo;
    /* 0x1F801DAA 0x1AA */ u16 spucnt;
    /* 0x1F801DAC 0x1AC */ u16 data_trans;
    /* 0x1F801DAE 0x1AE */ u16 spustat; // read-only on hardware
    /* 0x1F801DB0 0x1B0 */ SpuVolume cd_vol;
    /* 0x1F801DB4 0x1B4 */ SpuVolume ex_vol;
    /* 0x1F801DB8 0x1B8 */ SpuVolume main_volx;
    /* 0x1F801DBC 0x1BC */ SpuVolume unk_vol;
    /* 0x1F801DC0 0x1C0 */ u16 dAPF1;
    /* 0x1F801DC2 0x1C2 */ u16 dAPF2;
    /* 0x1F801DC4 0x1C4 */ u16 vIIR;
    /* 0x1F801DC6 0x1C6 */ u16 vCOMB1;
    /* 0x1F801DC8 0x1C8 */ u16 vCOMB2;
    /* 0x1F801DCA 0x1CA */ u16 vCOMB3;
    /* 0x1F801DCC 0x1CC */ u16 vCOMB4;
    /* 0x1F801DCE 0x1CE */ u16 vWALL;
    /* 0x1F801DD0 0x1D0 */ u16 vAPF1;
    /* 0x1F801DD2 0x1D2 */ u16 vAPF2;
    /* 0x1F801DD4 0x1D4 */ u16 mLSAME;
    /* 0x1F801DD6 0x1D6 */ u16 mRSAME;
    /* 0x1F801DD8 0x1D8 */ u16 mLCOMB1;
    /* 0x1F801DDA 0x1DA */ u16 mRCOMB1;
    /* 0x1F801DDC 0x1DC */ u16 mLCOMB2;
    /* 0x1F801DDE 0x1DE */ u16 mRCOMB2;
    /* 0x1F801DE0 0x1E0 */ u16 dLSAME;
    /* 0x1F801DE2 0x1E2 */ u16 dRSAME;
    /* 0x1F801DE4 0x1E4 */ u16 mLDIFF;
    /* 0x1F801DE6 0x1E6 */ u16 mRDIFF;
    /* 0x1F801DE8 0x1E8 */ u16 mLCOMB3;
    /* 0x1F801DEA 0x1EA */ u16 mRCOMB3;
    /* 0x1F801DEC 0x1EC */ u16 mLCOMB4;
    /* 0x1F801DEE 0x1EE */ u16 mRCOMB4;
    /* 0x1F801DF0 0x1F0 */ u16 dLDIFF;
    /* 0x1F801DF2 0x1F2 */ u16 dRDIFF;
    /* 0x1F801DF4 0x1F4 */ u16 mLAPF1;
    /* 0x1F801DF6 0x1F6 */ u16 mRAPF1;
    /* 0x1F801DF8 0x1F8 */ u16 mLAPF2;
    /* 0x1F801DFA 0x1FA */ u16 mRAPF2;
    /* 0x1F801DFC 0x1FC */ u16 vLIN;
    /* 0x1F801DFE 0x1FE */ u16 vRIN;
} SPU_RXX;

union SpuUnion {
    volatile SPU_RXX rxx;
    volatile u16 raw[0x100];
};

#ifndef __psyz
#define SPUR(field) (_spu_RXX->rxx.field)
#define SPUW(field, val) _spu_RXX->rxx.field = (val)
#else
#include <stddef.h>
#define SPUR(field) Psyz_SpuRead(offsetof(SPU_RXX, field))
#define SPUW(field, val) Psyz_SpuWrite(offsetof(SPU_RXX, field), val)
#endif

extern s32 D_80033098;
extern s32 D_80033550;
extern s32 D_8003355C;
extern s32 D_80033560;
extern s8* D_80033564;
extern int _spu_AllocBlockNum;
extern int _spu_AllocLastNum;
extern s32 _spu_EVdma;
extern s32 _spu_RQmask;
extern s32 _spu_RQvoice;
extern union SpuUnion* _spu_RXX;
extern s32 _spu_env;
extern int _spu_inTransfer;
extern s32 _spu_isCalled;
extern s32 _spu_keystat;
extern s32 _spu_mem_mode_plus;
extern s32 _spu_mem_mode_unitM;
extern SPU_MALLOC* _spu_memList;
extern s32 _spu_rev_flag;
extern s32 _spu_rev_offsetaddr;
extern s32 _spu_rev_reserve_wa;
extern s32 _spu_rev_startaddr[];
extern SpuReverbAttr _spu_rev_attr;
extern s32 _spu_transMode;
extern int _spu_trans_mode;
extern u16 _spu_tsa;
extern u16 _spu_voice_centerNote[];
extern void (* volatile _spu_transferCallback)();
extern void (* volatile _spu_IRQCallback)();

void* InterruptCallback(int, void (*)(void));
s32 SpuSetAnyVoice(s32 on_off, u32 bits, s32 addr1, s32 addr2);
void _SpuCallback(void (*cb)());
void _SpuInit(int bHot);
int _spu_init(int bHot);
s32 _SpuIsInAllocateArea_(u32);
void _spu_FiDMA(void);
unsigned _spu_Fw(unsigned char* addr, unsigned size);
unsigned _spu_Fr(unsigned char* addr, unsigned size);
void _spu_FsetRXX(unsigned offset, unsigned value, unsigned mode);
unsigned _spu_FsetRXXa(unsigned offset, unsigned unit);
void _spu_gcSPU(void);
int _spu_t(int arg0, ...);
s32 _spu_write(u8*, u32);
void _SpuDataCallback(SpuTransferCallbackProc func);
void _SsVmKeyOffNow(int mode);
int _spu_getInTransfer(void);
void _spu_FwriteByIO(unsigned char* addr, u_long size);
u_long _SpuSetAnyVoice(long on_off, u_long voice_bit, int arg2, int arg3);
void _spu_Fw1ts(void);

#endif
