#include "libsnd_private.h"

void _SsSndDecrescendo(short seq_access_num, short arg1) {
    struct SeqStruct* score = &_ss_score[seq_access_num][arg1];
    u16 voll, volr;

    score->v_time_r--;
    if (score->v_time_r < 0) {
        _ss_score[seq_access_num][arg1].flags &= ~0x20;
    } else if (score->unk4C > 0) {
        if ((score->v_time_r % score->unk4C) == 0) {
            score->vol_r--;
            if (score->vol_r > 0) {
                _SsVmGetSeqVol(seq_access_num | (arg1 << 8), &voll, &volr);
                if (voll - score->vol_r > 0 && volr - score->vol_r > 0 &&
                    voll != 1) {
                    _SsVmSetSeqVol(
                        seq_access_num | (arg1 << 8), voll - 1, volr - 1, 0);
                } else {
                    _SsVmSetSeqVol(seq_access_num | (arg1 << 8), 1, 1, 0);
                }
            } else {
                _ss_score[seq_access_num][arg1].flags &= ~0x20;
            }
            if ((score->v_time_r == 0) || (score->vol_r == 0)) {
                _ss_score[seq_access_num][arg1].flags &= ~0x20;
            }
        }
    } else {
        score->vol_r += score->unk4C;
        if (score->vol_r > 0) {
            _SsVmGetSeqVol(seq_access_num | (arg1 << 8), &voll, &volr);
            if (score->vol_l >=
                    (score->v_time_l - score->v_time_r) * -score->unk4C &&
                -score->unk4C < voll) {
                _SsVmSetSeqVol(seq_access_num | (arg1 << 8),
                               voll + score->unk4C, volr + score->unk4C, 0);
            } else {
                _SsVmSetSeqVol(seq_access_num | (arg1 << 8), 1, 1, 0);
            }
        } else {
            _ss_score[seq_access_num][arg1].flags &= ~0x20;
        }
        if (score->v_time_r == 0 || score->vol_r == 0) {
            _ss_score[seq_access_num][arg1].flags &= ~0x20;
        }
    }
    _SsVmGetSeqVol(seq_access_num | (arg1 << 8), &score->unk5C, &score->unk5E);
}