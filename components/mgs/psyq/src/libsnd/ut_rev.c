#include "libsnd_private.h"

short SsUtSetReverbType(short type) {
    short _type;
    int is_neg;

    is_neg = 0;
    _type = type;
    if (type < 0) {
        is_neg = 1;
        _type = -type;
    }

    if (_type >= 0 && _type < 10) {
        _svm_rattr.mask = 1;
        if (is_neg) {
            _svm_rattr.mode = _type | 0x100;
        } else {
            _svm_rattr.mode = _type;
        }
        if (_type == 0) {
            SpuSetReverb(0);
        }
        SpuSetReverbModeParam(&_svm_rattr);
        return _type;
    }
    return -1;
}

short SsUtGetReverbType(void) { return _svm_rattr.mode; }
