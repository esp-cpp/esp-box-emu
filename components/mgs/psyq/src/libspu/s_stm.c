#include "libspu_private.h"

int SpuSetTransferMode(int mode) {
    s32 transMode;

    switch (mode) {
    case SPU_TRANSFER_BY_DMA:
        transMode = 0;
        break;
    case SPU_TRANSFER_BY_IO:
        transMode = 1;
        break;
    default:
        transMode = 0;
    }
    _spu_trans_mode = mode;
    _spu_transMode = transMode;
    return transMode;
}
