#include "libsnd_private.h"

int _SsVmVSetUp(short vabId, short prog) {
    if (vabId < 0 || vabId >= NUM_VAB) {
        return -1;
    }
    if (_svm_vab_used[vabId] != 1) {
        return -1;
    }
    if (prog < kMaxPrograms) {
        _svm_vh = _svm_vab_vh[vabId];
        _svm_pg = _svm_vab_pg[vabId];
        _svm_tn = _svm_vab_tn[vabId];
        _svm_cur.vabId = vabId;
        _svm_cur.prog = prog;
        _svm_cur.field_7_fake_program = _svm_pg[prog].reserved1;
        return 0;
    }
    return -1;
}
