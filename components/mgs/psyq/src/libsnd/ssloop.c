#include "libsnd_private.h"

void SsSetLoop(short access_num, short seq_num, short l_count) {
    struct SeqStruct* score = &_ss_score[access_num][seq_num];
    score->unk20 = l_count;
    score->unk21 = 0;
}

short SsIsEos(short access_num, short seq_num) {
    struct SeqStruct* score = &_ss_score[access_num][seq_num];
    return score->play_mode;
}
