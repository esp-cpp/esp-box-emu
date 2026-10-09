#include "libsnd_private.h"

void SsVabClose(short vab_id) {
    if (vab_id >= 0 && vab_id < NUM_VAB) {
        if (_svm_vab_used[vab_id] == 1) {
            SpuFree(_svm_vab_start[vab_id]);
            _svm_vab_used[vab_id] = 0;
            _svm_vab_count--;
        }
    }
}
