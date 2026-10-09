#include "libsnd_private.h"

void SsSetTableSize(char* table, short s_max, short t_max) {
    s32 i, j;
    struct SeqStruct* pTypedTable;

    _snd_seq_s_max = s_max;
    _snd_seq_t_max = t_max;
    pTypedTable = (struct SeqStruct*)table;
    for (i = 0; i < s_max; i++) {
        _ss_score[i] = &pTypedTable[i * t_max];
    }
    for (i = 0; i < _snd_seq_s_max; i++) {
        for (j = 0; j < _snd_seq_t_max; j++) {
            _ss_score[i][j].flags = 0;
            _ss_score[i][j].unk22 = 0xFF;
            _ss_score[i][j].unk23 = 0;
            _ss_score[i][j].vol_l = 0;
            _ss_score[i][j].vol_r = 0;
            _ss_score[i][j].v_time_l = 0;
            _ss_score[i][j].v_time_r = 0;
            _ss_score[i][j].unk4C = 0;
            _ss_score[i][j].unkAC = 0;
            _ss_score[i][j].unkA8 = 0;
            _ss_score[i][j].unkA4 = 0;
            _ss_score[i][j].unk4E = 0;
            _ss_score[i][j].voll = 127;
            _ss_score[i][j].volr = 127;
            _ss_score[i][j].unk5C = 127;
            _ss_score[i][j].unk5E = 127;
        }
    }
}
