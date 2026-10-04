#include "libsnd_private.h"

static int vab_buffer = 0;
static short vabid_transfer = -1;

short SsVabTransBodyPartly(u_char* addr, u_long bufsize, short vabid) {
    unsigned new_bufsize;

    if (vabid < 0 || vabid > 0x10 || _svm_vab_used[vabid] != 2) {
        _spu_setInTransfer(0);
        return -1;
    }
    if (vab_buffer == 0) {
        vabid_transfer = vabid;
        vab_buffer = _svm_vab_total[vabid];
        SpuSetTransferMode(0);
        SpuSetTransferStartAddr((u_long)_svm_vab_start[vabid]);
    }
    if (vabid_transfer != vabid) {
        _spu_setInTransfer(0);
        return -1;
    }
    if (vab_buffer < bufsize) {
        new_bufsize = vab_buffer;
    } else {
        new_bufsize = bufsize;
    }
    _spu_setInTransfer(1);
    SpuWritePartly(addr, new_bufsize);
    vab_buffer -= new_bufsize;
    if (vab_buffer != 0) {
        return -2;
    }
    vabid_transfer = -1;
    vab_buffer = 0;
    _svm_vab_used[vabid] = 1;
    return vabid;
}
