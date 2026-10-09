#include <psyz.h>

static void DmaSpu(unsigned offset, u_long value) {
    static u_long _addr = 0;
    static unsigned _len = 0;
    static u_long memAddr;
    static unsigned spuAddr;

    switch (offset) {
    case 0:
        _addr = value;
        break;
    case 1:
        _len = (unsigned)((value >> 0x10) * (value & 0xFFFF) * sizeof(int));
        break;
    case 2: // chcr
        memAddr = _addr;
        spuAddr = (unsigned)Psyz_SpuRead(0x1A6) << 3;
        if (!(value & 0x300) != 0x200) {
            WARNF("SyncMode %d is invalid for SPU", (value >> 9) & 3);
            return;
        }
        if (value & 2) { // simulate backward direction
            memAddr -= _len;
        }
        if (value & 1) {
            Psyz_SpuMemWrite(spuAddr, (void*)memAddr, _len);
        } else {
            Psyz_SpuMemRead(spuAddr, (void*)memAddr, _len);
        }
        if (value & 0x01000000) {
            // DMA transfer is always synchronous on PC
        }
        break;
    }
}

void Psyz_DmaWrite(PsyzDmaChannel ch, unsigned offset, u_long value) {
    if (offset > 2) {
        WARNF("offset %d invalid for DMA channel %d", offset, ch);
        return;
    }
    switch (ch) {
    case DMA_CHANNEL_MDEC_IN:
        LOG_ONCE("DMA_CHANNEL_MDEC_IN not supported");
        break;
    case DMA_CHANNEL_MDEC_OUT:
        LOG_ONCE("DMA_CHANNEL_MDEC_OUT not supported");
        break;
    case DMA_CHANNEL_GPU:
        LOG_ONCE("DMA_CHANNEL_GPU not supported");
        break;
    case DMA_CHANNEL_CD:
        LOG_ONCE("DMA_CHANNEL_CD not supported");
        break;
    case DMA_CHANNEL_SPU:
        DmaSpu(offset, value);
        break;
    case DMA_CHANNEL_PIO:
        LOG_ONCE("DMA_CHANNEL_PIO not supported");
        break;
    case DMA_CHANNEL_OTC:
        LOG_ONCE("DMA_CHANNEL_OTC not supported");
        break;
    default:
        WARNF("DMA channel %d is invalid", ch);
        break;
    }
}
