#include "libspu_private.h"

SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc func) {
    SpuIRQCallbackProc prev;

    prev = _spu_IRQCallback;
    if (func != prev) {
        _spu_IRQCallback = func;
        _SpuCallback(func);
    }
    return prev;
}
