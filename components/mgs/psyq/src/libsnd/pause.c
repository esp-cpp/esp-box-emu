#include "libsnd_private.h"

void _SsSndPause(short seq_access_num, short arg1) {
    struct SeqStruct* score = &_ss_score[seq_access_num][arg1];
    _SsVmSeqKeyOff(arg1 << 8 | seq_access_num);
    score->play_mode = 0;
    _ss_score[seq_access_num][arg1].flags &= ~2;
}
