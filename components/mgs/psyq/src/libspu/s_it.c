#include "libspu_private.h"

void _spu_setInTransfer(s32 arg0) {
    if (arg0 == 1) {
        _spu_inTransfer = 0;
    } else {
        _spu_inTransfer = 1;
    }
}

int _spu_getInTransfer(void) { return _spu_inTransfer != 1; }
