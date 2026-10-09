#include "libsnd_private.h"

#ifdef __psyz
static
#endif
    inline void _SsSndSetCres(
        short sep_access_num, short seq_num, short vol, long v_time) {
    _SsSndSetVolData(sep_access_num, seq_num, vol, v_time);
    _ss_score[sep_access_num][seq_num].flags |= SEQ_FLAG_10;
    _ss_score[sep_access_num][seq_num].flags &= ~SEQ_FLAG_20;
}

void SsSeqSetCrescendo(short sep_access_num, short vol, long v_time) {
    _SsSndSetCres(sep_access_num, 0, vol, v_time);
}

void SsSepSetCrescendo(
    short sep_access_num, short seq_num, short vol, long v_time) {
    _SsSndSetCres(sep_access_num, seq_num, vol, v_time);
}
