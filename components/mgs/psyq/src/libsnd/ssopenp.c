#include "libsnd_private.h"

typedef void (*sCb)();

short SsSepOpen(unsigned long* addr, short vab_id, short seq_cnt) {
    short bit;
    short i;
    short flag;
    int len;
    unsigned char var_a3;

    flag = 0;
    if (_snd_openflag == -1) {
        printf("Can't Open Sequence data any more\n\n");
        return -1;
    }
    SsFCALL.noteon = (sCb)_SsNoteOn;
    SsFCALL.programchange = (sCb)_SsSetProgramChange;
    SsFCALL.metaevent = (sCb)_SsGetMetaEvent;
    SsFCALL.pitchbend = (sCb)_SsSetPitchBend;
    SsFCALL.control[CC_NUMBER] = (sCb)_SsSetControlChange;
    SsFCALL.control[CC_BANKCHANGE] = (sCb)_SsContBankChange;
    SsFCALL.control[CC_MAINVOL] = (sCb)_SsContMainVol;
    SsFCALL.control[CC_PANPOT] = (sCb)_SsContPanpot;
    SsFCALL.control[CC_EXPRESSION] = (sCb)_SsContExpression;
    SsFCALL.control[CC_DAMPER] = (sCb)_SsContDamper;
    SsFCALL.control[CC_NRPN1] = (sCb)_SsContNrpn1;
    SsFCALL.control[CC_NRPN2] = (sCb)_SsContNrpn2;
    SsFCALL.control[CC_RPN1] = (sCb)_SsContRpn1;
    SsFCALL.control[CC_RPN2] = (sCb)_SsContRpn2;
    SsFCALL.control[CC_EXTERNAL] = (sCb)_SsContExternal;
    SsFCALL.control[CC_RESETALL] = (sCb)_SsContResetAll;
    SsFCALL.control[CC_DATAENTRY] = (sCb)_SsContDataEntry;
    SsFCALL.ccentry[DE_PRIORITY] = (sCb)_SsSetNrpnVabAttr0;
    SsFCALL.ccentry[DE_MODE] = (sCb)_SsSetNrpnVabAttr1;
    SsFCALL.ccentry[DE_LIMITL] = (sCb)_SsSetNrpnVabAttr2;
    SsFCALL.ccentry[DE_LIMITH] = (sCb)_SsSetNrpnVabAttr3;
    SsFCALL.ccentry[DE_ADSR_AR_L] = (sCb)_SsSetNrpnVabAttr4;
    SsFCALL.ccentry[DE_ADSR_AR_E] = (sCb)_SsSetNrpnVabAttr5;
    SsFCALL.ccentry[DE_ADSR_DR] = (sCb)_SsSetNrpnVabAttr6;
    SsFCALL.ccentry[DE_ADSR_SL] = (sCb)_SsSetNrpnVabAttr7;
    SsFCALL.ccentry[DE_ADSR_SR_L] = (sCb)_SsSetNrpnVabAttr8;
    SsFCALL.ccentry[DE_ADSR_SR_E] = (sCb)_SsSetNrpnVabAttr9;
    SsFCALL.ccentry[DE_ADSR_RR_L] = (sCb)_SsSetNrpnVabAttr10;
    SsFCALL.ccentry[DE_ADSR_RR_E] = (sCb)_SsSetNrpnVabAttr11;
    SsFCALL.ccentry[DE_ADSR_SR] = (sCb)_SsSetNrpnVabAttr12;
    SsFCALL.ccentry[DE_VIB_TIME] = (sCb)_SsSetNrpnVabAttr13;
    SsFCALL.ccentry[DE_PORTA_DEPTH] = (sCb)_SsSetNrpnVabAttr14;
    SsFCALL.ccentry[DE_REV_TYPE] = (sCb)_SsSetNrpnVabAttr15;
    SsFCALL.ccentry[DE_REV_DEPTH] = (sCb)_SsSetNrpnVabAttr16;
    SsFCALL.ccentry[DE_ECHO_FB] = (sCb)_SsSetNrpnVabAttr17;
    SsFCALL.ccentry[DE_ECHO_DELAY] = (sCb)_SsSetNrpnVabAttr18;
    SsFCALL.ccentry[DE_DELAY] = (sCb)_SsSetNrpnVabAttr19;

    bit = 0;
    var_a3 = 0;
    do {
        if ((_snd_openflag & (1 << bit)) == 0U) {
            flag = bit;
            var_a3 = 1;
        }
        bit++;
    } while (var_a3 == 0);
    _snd_openflag |= 1 << flag;
    for (i = 0; i < seq_cnt; i++) {
        len = _SsInitSoundSep(flag, i, vab_id, addr);
        var_a3 = 1;
        if (len == -var_a3) {
            return -1;
        }
        addr = (unsigned long*)((unsigned char*)addr + len);
    }
    return flag;
}
