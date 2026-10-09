#include "libsnd_private.h"

void _SsSndSetVol(short sep_access_num, short seq_num, short voll, short volr) {
    _SsVmSetSeqVol(sep_access_num | (seq_num << 8), voll, volr, 0);
}

void SsSeqSetVol(short seq_access_num, short voll, short volr) {
    _SsVmSetSeqVol(seq_access_num, voll, volr, 0);
}

void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr) {
    _SsVmSetSeqVol(sep_access_num | (seq_num << 8), voll, volr, 0);
}

void SsSeqGetVol(short access_num, short seq_num, short* voll, short* volr) {
    _SsVmGetSeqVol(access_num | (seq_num << 8), voll, volr);
}
