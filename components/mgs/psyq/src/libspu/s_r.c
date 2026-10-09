#include "libspu_private.h"

u_long SpuRead(u_char* addr, u_long size) {
    if (size > 0x7EFF0) {
        size = 0x7EFF0;
    }
    _spu_Fr(addr, size);
    if (_spu_transferCallback == NULL) {
        _spu_inTransfer = 0;
    }
    return size;
}
