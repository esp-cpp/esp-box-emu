#include "libspu_private.h"

int _SpuMallocSeparateTo3(int arg0, unsigned addr, int size) {
    int temp_t1;
    int temp_t3;
    int temp_t4;
    int var_a3;
    int var_v1;
    int a1 = addr;

    temp_t4 = _spu_memList[arg0].size;
    temp_t3 = _spu_memList[arg0].addr;
    temp_t1 = a1 - (_spu_memList[arg0].addr & 0x0FFFFFFF);

    if (_spu_rev_reserve_wa == 0) {
        var_v1 = 0;
    } else {
        var_v1 = (0x10000 - _spu_rev_offsetaddr) << _spu_mem_mode_plus;
    }

    if (temp_t3 & 0x40000000) {
        var_a3 = (temp_t4 - temp_t1) - var_v1;
    } else {
        var_a3 = temp_t4 - temp_t1;
    }

    if (var_a3 < size) {
        return -1;
    }

    if (temp_t3 & 0x40000000) {
        if (temp_t1 > 0) {
            if ((_spu_AllocBlockNum - 2) < _spu_AllocLastNum) {
                return -1;
            }
            _spu_memList[_spu_AllocLastNum].size = temp_t1;
            _spu_memList[_spu_AllocLastNum].addr &= 0x0FFFFFFF;
            _spu_memList[_spu_AllocLastNum].addr |= 0x80000000;
            _spu_AllocLastNum++;
            _spu_memList[_spu_AllocLastNum].addr = a1;
            _spu_memList[_spu_AllocLastNum].size = size;
        } else {
            if ((_spu_AllocBlockNum - 1) < _spu_AllocLastNum) {
                return -1;
            }
            _spu_memList[_spu_AllocLastNum].size = size;
            _spu_memList[_spu_AllocLastNum].addr &= 0x0FFFFFFF;
        }

        _spu_AllocLastNum++;
        _spu_memList[_spu_AllocLastNum].addr =
            (temp_t3 & 0x0FFFFFFF) + temp_t1 + size;
        _spu_memList[_spu_AllocLastNum].addr |= 0x40000000;
        _spu_memList[_spu_AllocLastNum].size = (temp_t4 - temp_t1) - size;
        _spu_gcSPU();
        return a1;
    }
    if (temp_t1 > 0) {
        if (var_a3 == size) {
            if ((_spu_AllocBlockNum - 2) < _spu_AllocLastNum) {
                return -1;
            }
        } else {
            if ((_spu_AllocBlockNum - 1) < _spu_AllocLastNum) {
                return -1;
            }
        }
        _spu_memList[arg0].size = temp_t1;
        {
            int temp_v1_3 = _spu_memList[_spu_AllocLastNum].addr;
            int var_a0 = _spu_memList[_spu_AllocLastNum].size;
            _spu_memList[_spu_AllocLastNum].addr = a1;
            _spu_memList[_spu_AllocLastNum].size = size;
            _spu_memList[_spu_AllocLastNum + 1].addr = temp_v1_3;
            _spu_memList[_spu_AllocLastNum + 1].size = var_a0;
            _spu_AllocLastNum++;
            if (var_a3 >= size) {
                unsigned swapAddr = _spu_memList[_spu_AllocLastNum].addr;
                unsigned swapSize = _spu_memList[_spu_AllocLastNum].size;
                _spu_memList[_spu_AllocLastNum].addr =
                    (temp_t3 & 0x0FFFFFFF) + temp_t1 + size;
                _spu_memList[_spu_AllocLastNum].addr |= 0x80000000;
                _spu_memList[_spu_AllocLastNum].size = var_a3 - size;
                _spu_memList[_spu_AllocLastNum + 1].addr = swapAddr;
                _spu_memList[_spu_AllocLastNum + 1].size = swapSize;
                _spu_AllocLastNum++;
            }

            _spu_gcSPU();
            return a1;
        }
    } else {
        if ((size < var_a3) && ((_spu_AllocBlockNum - 1) < _spu_AllocLastNum)) {
            return -1;
        }

        _spu_memList[arg0].size = size;
        _spu_memList[arg0].addr &= 0x0FFFFFFF;
        if (size < var_a3) {
            unsigned swapAddr = _spu_memList[_spu_AllocLastNum].addr;
            unsigned swapSize = _spu_memList[_spu_AllocLastNum].size;

            _spu_memList[_spu_AllocLastNum].addr =
                (temp_t3 & 0x0FFFFFFF) + size;
            _spu_memList[_spu_AllocLastNum].addr |= 0x80000000;
            _spu_memList[_spu_AllocLastNum].size = var_a3 - size;
            _spu_memList[_spu_AllocLastNum + 1].addr = swapAddr;
            _spu_memList[_spu_AllocLastNum + 1].size = swapSize;
            _spu_AllocLastNum++;
        }

        _spu_gcSPU();
        return a1;
    }

    return -1;
}

int SpuMallocWithStartAddr(unsigned _addr, int size) {
    int temp_a2;
    int addr;
    int var_s2;
    int i;

    if (_spu_rev_reserve_wa == 0) {
        var_s2 = 0;
    } else {
        var_s2 = (0x10000 - _spu_rev_offsetaddr) << _spu_mem_mode_plus;
    }

    size += (size & ~_spu_mem_mode_unitM) ? _spu_mem_mode_unitM : 0;
    size >>= _spu_mem_mode_plus;
    size <<= _spu_mem_mode_plus;

    addr = _addr;
    addr += (addr & ~_spu_mem_mode_unitM) ? _spu_mem_mode_unitM : 0;
    addr >>= _spu_mem_mode_plus;
    addr <<= _spu_mem_mode_plus;

    if (addr < 0x1010) {
        return -1;
    }

    if (addr + size > (0x10000 << _spu_mem_mode_plus) - var_s2) {
        return -1;
    }

    if (_spu_memList[0].addr & 0x40000000) {
        temp_a2 = _spu_memList[0].size - (addr - 0x1010) - size;
        if ((addr - 0x1010) > 0) {
            if ((_spu_AllocBlockNum - 2) < _spu_AllocLastNum) {
                return -1;
            }
            _spu_memList[0].addr = 0x80001010;
            _spu_memList[0].size = addr - 0x1010;

            _spu_memList[1].addr = addr;
            _spu_memList[1].size = size;

            _spu_memList[2].addr = (addr + size) | 0x40000000;
            _spu_memList[2].size = temp_a2;

            _spu_AllocLastNum = 2;
        } else {
            if ((_spu_AllocBlockNum - 1) < _spu_AllocLastNum) {
                return -1;
            }
            _spu_memList[0].addr = addr;
            _spu_memList[0].size = size;

            _spu_memList[1].addr = (addr + size) | 0x40000000;
            _spu_memList[1].size = temp_a2;

            _spu_AllocLastNum = 1;
        }
        _spu_gcSPU();
        return addr;
    }

    _spu_gcSPU();
    for (i = 0;;) {
        if (_spu_memList[i].addr & 0x80000000) {
            if (addr >= (_spu_memList[i].addr & 0x0FFFFFFF)) {
                if (addr >= (_spu_memList[i].addr & 0x0FFFFFFF) +
                                _spu_memList[i].size) {
                    if (!(_spu_memList[i + 1].addr & 0x40000000)) {
                        i++;
                        continue;
                    }
                }
                temp_a2 = _spu_memList[i].size -
                          (addr - (_spu_memList[i].addr & 0x0FFFFFFF));
                if (temp_a2 < size) {
                    i++;
                    continue;
                }
                addr = _SpuMallocSeparateTo3(i, addr, size);
                break;
            }
            temp_a2 = _spu_memList[i].size;
            if (temp_a2 < size) {
                i++;
                continue;
            }
            addr = _SpuMallocSeparateTo3(
                i, _spu_memList[i].addr & 0x0FFFFFFF, size);
            break;
        }
        if (!(_spu_memList[i].addr & 0x40000000)) {
            i++;
            continue;
        } else {
            temp_a2 = _spu_memList[i].size - var_s2;
            if (temp_a2 < size) {
                return -1;
            }

            addr = _SpuMallocSeparateTo3(
                i, _spu_memList[i].addr & 0x0FFFFFFF, size);
            break;
        }
    }

    _spu_gcSPU();
    return addr;
}
