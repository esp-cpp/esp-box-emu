#include "libsnd_private.h"

void _SsSndSetVolData(
    short sep_access_num, short seq_num, short vol, int v_time) {
    struct SeqStruct* score;
    int vol_abs;

    score = &_ss_score[sep_access_num][seq_num];
    if (score->flags & SEQ_FLAG_4) {
        return;
    }
    if (score->flags & SEQ_FLAG_100) {
        return;
    }
    if (vol == 0) {
        return;
    }
    vol_abs = vol > 0 ? vol : -vol;
    score->vol_l = vol;
    score->v_time_l = v_time;
    score->vol_r = vol;
    score->v_time_r = v_time;
    if (v_time >= vol_abs) {
        score->unk4C = v_time / vol_abs;
    } else {
        score->unk4C = -(vol_abs / v_time);
    }
}
