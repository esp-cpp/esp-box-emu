#include "libsnd_private.h"

void _SsSndReplay(short seq_access_num, short arg1) {
    struct SeqStruct* score = &_ss_score[seq_access_num][arg1];
    score->play_mode = 1;
    _ss_score[seq_access_num][arg1].flags &= ~8;
}
