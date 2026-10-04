#include "libspu_private.h"

int SpuGetTransferMode(void) {
    if (_spu_transMode && _spu_transMode == 1) {
        _spu_trans_mode = _spu_transMode;
    } else {
        _spu_trans_mode = 0;
    }
    return _spu_trans_mode;
}
