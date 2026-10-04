#include "libsnd_private.h"

void _SsSndCrescendo(short seq_access_num, short arg1) {
    struct SeqStruct* score = &_ss_score[seq_access_num][arg1];
    unsigned short voll, volr;

    score->v_time_r--;
    if (score->v_time_r < 0) {
        _ss_score[seq_access_num][arg1].flags &= ~0x10;
    } else if (score->unk4C > 0) {
        if ((score->v_time_r % score->unk4C) == 0) {
            score->vol_r--;
            if (score->vol_r >= 0) {
                _SsVmGetSeqVol(seq_access_num | (arg1 << 8), &voll, &volr);
                if (voll + 1 <= voll + score->vol_r) {
                    _SsVmSetSeqVol(
                        seq_access_num | (arg1 << 8), voll + 1, volr + 1, 0);
                }
            } else {
                _SsVmSetSeqVol(seq_access_num | (arg1 << 8), 127, 127, 0);
                _ss_score[seq_access_num][arg1].flags &= ~0x10;
            }
            if (score->v_time_r == 0 || score->vol_r == 0) {
                _ss_score[seq_access_num][arg1].flags &= ~0x10;
            }
        }
    } else if (score->unk4C < 0) {
        score->vol_r += score->unk4C;
        if (score->vol_r >= 0) {
            _SsVmGetSeqVol(seq_access_num | (arg1 << 8), &voll, &volr);
            if (voll - score->unk4C >= 127 && volr - score->unk4C >= 127) {
                _SsVmSetSeqVol(seq_access_num | (arg1 << 8), 127, 127, 0);
            }
            if ((score->v_time_l - score->v_time_r) * -score->unk4C <
                score->vol_l) {
                _SsVmSetSeqVol(seq_access_num | (arg1 << 8),
                               voll - score->unk4C, volr - score->unk4C, 0);
            }
        } else {
            _SsVmSetSeqVol(seq_access_num | (arg1 << 8), 127, 127, 0);
            _ss_score[seq_access_num][arg1].flags &= ~0x10;
        }
        if (score->v_time_r == 0 || score->vol_r == 0) {
            _ss_score[seq_access_num][arg1].flags &= ~0x10;
        }
    }
    _SsVmGetSeqVol(seq_access_num | (arg1 << 8), &score->unk5C, &score->unk5E);
}
