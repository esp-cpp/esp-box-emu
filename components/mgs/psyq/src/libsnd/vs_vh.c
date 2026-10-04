#include "libsnd_private.h"

short SsVabOpenHeadWithMode(
    u_char* addr, short vabid, short mode, u_long sbaddr);

short SsVabOpenHead(u_char* addr, short vabid) {
    return SsVabOpenHeadWithMode(addr, vabid, 0, 0);
}

short SsVabOpenHeadSticky(u_char* addr, short vabid, u_long sbaddr) {
    return SsVabOpenHeadWithMode(addr, vabid, 1, sbaddr);
}

short SsVabFakeHead(u_char* addr, short vabid, u_long sbaddr) {
    return SsVabOpenHeadWithMode(addr, vabid, 1, sbaddr);
}

short SsVabOpenHeadWithMode(
    u_char* addr, short vabid, short mode, u_long sbaddr) {
    int vagLens[256];
    int i;
    int size;
    short vab_id;
    unsigned short vag_header_len;
    unsigned short* ptr_vag_off_table;
    unsigned int magic;
    unsigned spuAllocMem;
    unsigned char num_vags;
    unsigned char* ptr;
    ProgAtr* progAtr;
    VabHdr* vab_header;

    vab_id = NUM_VAB;
#ifndef __psyz
    // TODO this locks on Psy-Z
    if (_spu_getInTransfer() == 1) {
        return -1;
    }
#endif
    _spu_setInTransfer(1);
    if (vabid >= NUM_VAB) {
        _spu_setInTransfer(0);
        return -1;
    }
    if (vabid == -1) {
        for (i = 0; i < NUM_VAB; i++) {
            if (_svm_vab_used[i] == 0) {
                _svm_vab_used[i] = 1;
                vab_id = i;
                _svm_vab_count++;
                break;
            }
        }
    } else {
        ptr = _svm_vab_used;
        if (ptr[vabid] == 0) {
            _svm_vab_used[vabid] = 1;
            vab_id = vabid;
            _svm_vab_count++;
        }
    }
    if (vab_id >= 0x10) {
        _spu_setInTransfer(0);
        return -1;
    }

    ptr = addr;
    _svm_vab_vh[vab_id] = (VabHdr*)ptr;
    ptr += sizeof(VabHdr);
    vab_header = (VabHdr*)addr;
    magic = vab_header->form;
    if ((magic >> 8) != ('V' << 0x10 | 'A' << 0x8 | 'B')) {
        _svm_vab_used[vab_id] = 0;
        _spu_setInTransfer(0);
        _svm_vab_count--;
        return -1;
    }
    if ((magic & 0xFF) == 'p') {
        if (vab_header->ver >= 5) {
            kMaxPrograms = 0x80;
        } else {
            kMaxPrograms = 0x40;
        }
    } else {
        kMaxPrograms = 0x40;
    }
    if (vab_header->ps > kMaxPrograms) {
        _svm_vab_used[vab_id] = 0;
        _spu_setInTransfer(0);
        _svm_vab_count--;
        return -1;
    }

    _svm_vab_pg[vab_id] = (ProgAtr*)ptr;
    progAtr = (ProgAtr*)ptr;
    ptr += kMaxPrograms * 0x10;
    size = 0;
    for (i = 0; i < kMaxPrograms; i++) {
        progAtr[i].reserved1 = size;
        if (progAtr[i].tones != 0) {
            size++;
        }
    }
    size = 0;
    _svm_vab_tn[vab_id] = (VagAtr*)ptr;
    ptr_vag_off_table = (u16*)(ptr + vab_header->ps * 512);
    num_vags = vab_header->vs;
    for (i = 0; i < LEN(vagLens); i++) {
        if (num_vags >= i) {
            vag_header_len = *ptr_vag_off_table;
            if (vab_header->ver >= 5) {
                vagLens[i] = vag_header_len * 8;
            } else {
                vagLens[i] = vag_header_len * 4;
            }
            size += vagLens[i];
        }
        ptr_vag_off_table++;
    }

    if (mode == 0) {
        spuAllocMem = (unsigned)SpuMalloc(size);
        if (spuAllocMem == -1) {
            _svm_vab_used[vab_id] = 0;
            _spu_setInTransfer(0);
            _svm_vab_count--;
            return -1;
        }
    } else {
        spuAllocMem = sbaddr;
    }
    if (spuAllocMem + size > 0x80000) {
        _svm_vab_used[vab_id] = 0;
        _spu_setInTransfer(0);
        _svm_vab_count--;
        return -1;
    }
    _svm_vab_start[vab_id] = spuAllocMem;
    size = 0;
    for (i = 0; i <= num_vags; i++) {
        size += vagLens[i];
        if (!(i & 1)) {
            ((short*)&progAtr[i / 2].reserved2)[0] = (spuAllocMem + size) / 8;
        } else {
            ((short*)&progAtr[i / 2].reserved2)[1] = (spuAllocMem + size) / 8;
        }
    }

    _svm_vab_total[vab_id] = size;
    _svm_vab_used[vab_id] = 2;
    return vab_id;
}
