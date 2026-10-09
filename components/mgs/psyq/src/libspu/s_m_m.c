#include "libspu_private.h"

int SpuMalloc(int size) {
    int var_s2;
    int var_s3;
    int i;

    i = 0;
    var_s2 = -1;
    if (_spu_rev_reserve_wa == 0) {
        var_s3 = 0;
    } else {
        var_s3 = (0x10000 - _spu_rev_offsetaddr) << _spu_mem_mode_plus;
    }
    size += (size & ~_spu_mem_mode_unitM) ? _spu_mem_mode_unitM : 0;
    size >>= _spu_mem_mode_plus;
    size <<= _spu_mem_mode_plus;
    if (_spu_memList[0].addr & 0x40000000) {
        var_s2 = 0;
    } else {
        _spu_gcSPU();
        for (; i < _spu_AllocBlockNum; i++) {
            if (_spu_memList[i].addr & 0x40000000 ||
                (_spu_memList[i].addr & 0x80000000 &&
                 _spu_memList[i].size >= size)) {
                var_s2 = i;
                break;
            }
        }
    }
    if (var_s2 == -1) {
        return -1;
    }
    if (_spu_memList[var_s2].addr & 0x40000000) {
        if (var_s2 < _spu_AllocBlockNum &&
            _spu_memList[var_s2].size - var_s3 >= size) {
            int next = var_s2 + 1;

            _spu_memList[next].addr =
                (*(volatile int*)&_spu_memList[var_s2].addr & 0x0FFFFFFF) +
                    size |
                0x40000000;
            _spu_memList[next].size = _spu_memList[var_s2].size - size;
            _spu_memList[var_s2].addr &= 0x0FFFFFFF;
            _spu_memList[var_s2].size = size;
            _spu_AllocLastNum = next;
            _spu_gcSPU();
            return _spu_memList[var_s2].addr;
        }
    } else {
        if (size < _spu_memList[var_s2].size &&
            _spu_AllocLastNum < _spu_AllocBlockNum) {
            unsigned _addr = _spu_memList[var_s2].addr + size;
            unsigned _size = _spu_memList[var_s2].size - size;
            unsigned swapAddr = _spu_memList[_spu_AllocLastNum].addr;
            unsigned swapSize = _spu_memList[_spu_AllocLastNum].size;

            _spu_memList[_spu_AllocLastNum].addr = _addr | 0x80000000;
            _spu_memList[_spu_AllocLastNum].size = _size;
            _spu_memList[_spu_AllocLastNum + 1].addr = swapAddr;
            _spu_memList[_spu_AllocLastNum + 1].size = swapSize;
            _spu_AllocLastNum++;
        }
        _spu_memList[var_s2].size = size;
        _spu_memList[var_s2].addr &= 0x0FFFFFFF;
        _spu_gcSPU();
        return _spu_memList[var_s2].addr;
    }
    return -1;
}
