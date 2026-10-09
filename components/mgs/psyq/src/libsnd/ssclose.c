#include "libsnd_private.h"

void _SsClose(s16 seq_sep_num) {
    s32 seq_num;
    _SsVmSetSeqVol(seq_sep_num, 0, 0, 1);
    _SsVmSeqKeyOff(seq_sep_num);
    _snd_openflag &= ~(1 << seq_sep_num);
    for (seq_num = 0; seq_num < _snd_seq_t_max; seq_num++) {
        _ss_score[seq_sep_num][seq_num].flags = 0;
        _ss_score[seq_sep_num][seq_num].unk22 = 0xFF;
        _ss_score[seq_sep_num][seq_num].unk23 = 0;
        _ss_score[seq_sep_num][seq_num].vol_l = 0;
        _ss_score[seq_sep_num][seq_num].vol_r = 0;
        _ss_score[seq_sep_num][seq_num].v_time_l = 0;
        _ss_score[seq_sep_num][seq_num].v_time_r = 0;
        _ss_score[seq_sep_num][seq_num].unk4C = 0;
        _ss_score[seq_sep_num][seq_num].unkAC = 0;
        _ss_score[seq_sep_num][seq_num].unkA8 = 0;
        _ss_score[seq_sep_num][seq_num].unkA4 = 0;
        _ss_score[seq_sep_num][seq_num].unk4E = 0;
        _ss_score[seq_sep_num][seq_num].voll = 127;
        _ss_score[seq_sep_num][seq_num].volr = 127;
    }
}

void SsSeqClose(short seq_access_num) { _SsClose(seq_access_num); }

void SsSepClose(short sep_access_num) { _SsClose(sep_access_num); }
