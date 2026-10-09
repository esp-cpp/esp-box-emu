#include "libsnd_private.h"

void Snd_SetPlayMode(
    short sep_access_num, short seq_num, u8 play_mode, short l_count) {
    struct SeqStruct* score = &_ss_score[sep_access_num][seq_num];

    score->unk0 = score->read_pos;
    score->next_sep_pos = score->read_pos;
    score->loop_pos = score->read_pos;
    _ss_score[sep_access_num][seq_num].flags &= ~SEQ_FLAG_200;
    _ss_score[sep_access_num][seq_num].flags &= ~SEQ_FLAG_4;
    score->unk20 = l_count;

    if (play_mode == 1) {
        _ss_score[sep_access_num][seq_num].flags |= SEQ_FLAG_1;
        score->unk21 = 0;
        score->play_mode = SSPLAY_PLAY;
        _SsVmSetSeqVol(
            sep_access_num | (seq_num << 8), score->voll, score->volr, 0);
    } else if (play_mode == 0) {
        _ss_score[sep_access_num][seq_num].flags |= SEQ_FLAG_2;
    }
}