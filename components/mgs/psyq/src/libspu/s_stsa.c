#include "libspu_private.h"

unsigned SpuSetTransferStartAddr(unsigned addr) {
    unsigned _addr;
    _addr = addr;
    if (_addr - 0x1010 > 0x7EFE8) {
        return 0;
    }
    _addr = _spu_FsetRXXa(-1, _addr);
    _spu_tsa = (u16)_addr;
    return (u16)_addr << _spu_mem_mode_plus;
}
