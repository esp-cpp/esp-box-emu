#include <libapi.h>
#include <stdarg.h>
#include "libspu_private.h"

#ifndef __psyz
union SpuUnion* _spu_RXX = (union SpuUnion*)0x1F801C00;
static volatile unsigned* dma_spu_madr = (unsigned*)0x1F8010C0;
static volatile unsigned* dma_spu_bcr = (unsigned*)0x1F8010C4;
static volatile unsigned* dma_spu_chcr = (unsigned*)0x1F8010C8;
static volatile unsigned* dma_dpcr = (unsigned*)0x1F8010F0;
static volatile unsigned* spu_delay = (unsigned*)0x1F801014;
#else
extern unsigned* dma_dpcr;
extern unsigned* spu_delay;
#endif

u16 _spu_tsa = 0;
static u16 dummy = 0;
int _spu_transMode = 0;
int _spu_addrMode = 0;
int _spu_mem_mode = 2;
int _spu_mem_mode_plus = 3;
int _spu_mem_mode_unit = 8;
int _spu_mem_mode_unitM = 7;
int _spu_inTransfer = 1;
void (* volatile _spu_transferCallback)() = NULL;
void (* volatile _spu_IRQCallback)() = NULL;
s8 _spu_dummy[] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
static int D_800D1058 = 0;
static int spu_madr = 0;
static int spu_bcr = 0;

extern volatile u16 _spu_RQ[10];

int _spu_init(int bHot) {
    unsigned dmaTimer;
    int i;

    *dma_dpcr |= DMA_DPCR_SPU_PRIORITY_HIGH | DMA_DPCR_MASK_DMA4_ENABLE;
    SPUW(main_vol.left, 0);
    SPUW(main_vol.right, 0);
    SPUW(spucnt, 0);
    _spu_transMode = 0;
    _spu_addrMode = 0;
    _spu_tsa = 0;
    _spu_Fw1ts();
    SPUW(main_vol.left, 0);
    SPUW(main_vol.right, 0);

    dmaTimer = 0;
    while (SPUR(spustat) & 0x7FF) {
        if (++dmaTimer > DMA_TIMEOUT) {
            printf("SPU:T/O [%s]\n", "wait (reset)");
            break;
        }
    }

    _spu_mem_mode = 2;
    _spu_mem_mode_plus = 3;
    _spu_mem_mode_unit = 8;
    _spu_mem_mode_unitM = 7;
    SPUW(data_trans, 4);
    SPUW(rev_vol.left, 0);
    SPUW(rev_vol.right, 0);
    SPUW(key_off[0], 0xFFFF);
    SPUW(key_off[1], 0xFFFF);
    SPUW(rev_mode[0], 0);
    SPUW(rev_mode[1], 0);
    for (i = 0; i < 10; i++) {
        _spu_RQ[i] = 0;
    }
    if (!bHot) {
        SPUW(chan_fm[0], 0);
        SPUW(chan_fm[1], 0);
        SPUW(noise_mode[0], 0);
        SPUW(noise_mode[1], 0);
        SPUW(cd_vol.left, 0);
        SPUW(cd_vol.right, 0);
        SPUW(ex_vol.left, 0);
        SPUW(ex_vol.right, 0);
        _spu_tsa = 0x200;
        _spu_FwriteByIO((unsigned char*)&_spu_dummy, LEN(_spu_dummy));
        for (i = 0; i < NUM_VOICES; i++) {
            SPUW(voice[i].volume.left, 0);
            SPUW(voice[i].volume.right, 0);
            SPUW(voice[i].pitch, 0x3fff);
            SPUW(voice[i].addr, 0x200);
            SPUW(voice[i].adsr[0], 0);
            SPUW(voice[i].adsr[1], 0);
        }
        SPUW(key_on[0], 0xFFFF);
        SPUW(key_on[1], 0xFF);
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        SPUW(key_off[0], 0xFFFF);
        SPUW(key_off[1], 0xFF);
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
    }
    SPUW(spucnt, SPU_CTRL_MASK_MUTE_SPU | SPU_CTRL_MASK_SPU_ENABLE);
    _spu_inTransfer = 1;
    _spu_transferCallback = NULL;
    _spu_IRQCallback = NULL;
    return 0;
}

void _spu_FwriteByIO(unsigned char* addr, u_long size) {
    unsigned short spustat;
    int num_to_trans;
    unsigned short* cur_pos;
    int spustat_cur;
    int i;
    unsigned short cnt;
    unsigned timeout;

    cur_pos = (unsigned short*)addr;
    spustat = SPUR(spustat) & 0x7FF;
    SPUW(trans_addr, _spu_tsa);
    _spu_Fw1ts();
    while (size > 0) {
        num_to_trans = (size > 0x40) ? 0x40 : size;
        for (i = 0; i < num_to_trans; i += 2) {
            SPUW(trans_fifo, *cur_pos++);
        }
        cnt = SPUR(spucnt);
        cnt &= ~SPU_CTRL_MASK_SRAM_TRANSFER_MODE;
        cnt |= SPU_CTRL_MASK_TRANSFER_MANUAL_WRITE;
        SPUW(spucnt, cnt);
        _spu_Fw1ts();
        timeout = 0;
        while (SPUR(spustat) & 0x400) {
            timeout++;
            if (timeout > 0xF00) {
                printf("SPU:T/O [%s]\n", "wait (wrdy H -> L)");
                break;
            }
        }
        _spu_Fw1ts();
        _spu_Fw1ts();
        size -= num_to_trans;
    }
    cnt = SPUR(spucnt);
    cnt &= ~SPU_CTRL_MASK_SRAM_TRANSFER_MODE;
    SPUW(spucnt, cnt);
    timeout = 0;
    spustat_cur = SPUR(spustat) & 0x7FF;
    while (spustat_cur != spustat) {
        timeout++;
        if (timeout > 0xF00) {
            printf("SPU:T/O [%s]\n", "wait (dmaf clear/W)");
            return;
        }
        spustat_cur = SPUR(spustat) & 0x7FF;
    }
}

void _spu_FiDMA(void) {
    unsigned timeout;
    unsigned short cnt;

    if (D_800D1058 == 0) {
        _spu_Fw1ts();
    }
    SPUW(spucnt, SPUR(spucnt) & ~SPU_CTRL_MASK_SRAM_TRANSFER_MODE);
    timeout = 0;
    while (SPUR(spucnt) & SPU_CTRL_MASK_SRAM_TRANSFER_MODE) {
        timeout++;
        if (timeout > 0xF00) {
            break;
        }
    }
    if (_spu_transferCallback) {
        _spu_transferCallback();
        return;
    }
    DeliverEvent(0xF0000009, 0x20);
}

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fr_);

#ifndef __psyz
int _spu_t(int arg0, ...) {
    unsigned channelControl;
    unsigned i;
    unsigned addr;
    va_list args;
    unsigned arg;
    u16 mode;
    u16 cnt;

    va_start(args, arg0);
    switch (arg0) {
    case 2:
        arg = va_arg(args, unsigned);
        _spu_tsa = arg >> _spu_mem_mode_plus;
        _spu_RXX->rxx.trans_addr = _spu_tsa;
        break;
    case 1:
        D_800D1058 = 0;
        i = 0;
        while (_spu_RXX->rxx.trans_addr != _spu_tsa) {
            if (++i > 0xF00) {
                return -2;
            }
        }
        cnt = _spu_RXX->rxx.spucnt;
        cnt &= ~SPU_CTRL_MASK_SRAM_TRANSFER_MODE;
        cnt |= SPU_CTRL_MASK_TRANSFER_DMA_WRITE;
        _spu_RXX->rxx.spucnt = cnt;
        break;
    case 0:
        D_800D1058 = 1;
        i = 0;
        while (_spu_RXX->rxx.trans_addr != _spu_tsa) {
            if (++i > 0xF00) {
                return -2;
            }
        }
        cnt = _spu_RXX->rxx.spucnt;
        cnt &= ~SPU_CTRL_MASK_SRAM_TRANSFER_MODE;
        cnt |= SPU_CTRL_MASK_TRANSFER_DMA_READ;
        _spu_RXX->rxx.spucnt = cnt;
        break;
    case 3:
        if (D_800D1058 == 1) {
            mode = SPU_CTRL_MASK_TRANSFER_DMA_READ;
        } else {
            mode = SPU_CTRL_MASK_TRANSFER_DMA_WRITE;
        }
        i = 0;
        while (
            (_spu_RXX->rxx.spucnt & SPU_CTRL_MASK_SRAM_TRANSFER_MODE) != mode) {
            if (++i > 0xF00) {
                return -2;
            }
        }
        if (D_800D1058 == 1) {
            _spu_FsetDelayR();
        } else {
            _spu_FsetDelayW();
        }
        arg = va_arg(args, unsigned);
        spu_madr = arg;
        arg = va_arg(args, unsigned);
        spu_bcr = arg / 0x40;
        spu_bcr += (arg % 0x40) ? 1 : 0;
        *dma_spu_madr = spu_madr;
        *dma_spu_bcr = (spu_bcr << 0x10) | 0x10;
        if (D_800D1058 == 1) {
            channelControl = 0x01000200;
        } else {
            channelControl = 0x01000201;
        }
        *dma_spu_chcr = channelControl;
        break;
    }
    return 0;
}
#endif

unsigned _spu_Fw(unsigned char* addr, unsigned size) {
    if (_spu_transMode) {
        _spu_FwriteByIO(addr, size);
        return size;
    }
#ifdef __psyz
    SPUW(trans_addr, _spu_tsa);
    Psyz_SpuMemWrite(_spu_tsa << _spu_mem_mode_plus, addr, size);
#else
    _spu_t(2, _spu_tsa << _spu_mem_mode_plus);
    _spu_t(1);
    _spu_t(3, addr, size);
#endif
    return size;
}

unsigned _spu_Fr(unsigned char* addr, unsigned size) {
#ifdef __psyz
    SPUW(trans_addr, _spu_tsa);
    Psyz_SpuMemRead(_spu_tsa << _spu_mem_mode_plus, addr, size);
#else
    _spu_t(2, _spu_tsa << _spu_mem_mode_plus);
    _spu_t(0);
    _spu_t(3, addr, size);
#endif
    return size;
}

void _spu_FsetRXX(unsigned offset, unsigned value, unsigned mode) {
#ifdef __psyz
    if (mode == 0) {
        Psyz_SpuWrite(offset * sizeof(short), value);
    } else {
        Psyz_SpuWrite(offset * sizeof(short), value >> _spu_mem_mode_plus);
    }
#else
    if (mode == 0) {
        _spu_RXX->raw[offset] = value;
    } else {
        _spu_RXX->raw[offset] = value >> _spu_mem_mode_plus;
    }
#endif
}

unsigned _spu_FsetRXXa(unsigned offset, unsigned unit) {
    unsigned short value;

    if (_spu_mem_mode && (unit % _spu_mem_mode_unit)) {
        unit += _spu_mem_mode_unit;
        unit &= ~_spu_mem_mode_unitM;
    }
    value = unit >> _spu_mem_mode_plus;

    switch (offset) {
    case -1:
        return value & 0xFFFF;
    case -2:
        return unit;
    default:
#ifdef __psyz
        Psyz_SpuWrite(offset * sizeof(short), value);
#else
        _spu_RXX->raw[offset] = value;
#endif
        return unit;
    }
}

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FgetRXXa);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetPCR);

#ifndef __psyz
static inline void _spu_FsetDelayW(void) {
    *spu_delay = (*spu_delay & 0xF0FFFFFF) | 0x20000000;
}

static void _spu_FsetDelayR(void) {
    *spu_delay = (*spu_delay & 0xF0FFFFFF) | 0x22000000;
}
#endif

void _spu_Fw1ts(void) {
#ifndef __psyz
    volatile int i;
    volatile int sp4;

    sp4 = 13;
    for (i = 0; i < 60; i++) {
        sp4 *= 13;
    }
#endif
}
