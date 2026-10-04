#include "libspu_private.h"

void SpuSetCommonAttr(SpuCommonAttr* attr) {
    u16 mvol_mode_left;
    u16 mvol_mode_right;
    u16 vol_total_left;
    u16 vol_total_right;
    u32 mask;
    s32 bSetAll;
    u16 cnt;

    vol_total_left = 0;
    vol_total_right = 0;
    mask = attr->mask;
    bSetAll = attr->mask == 0;

    if (bSetAll || mask & SPU_COMMON_MVOLL) {
        if (bSetAll || mask & SPU_COMMON_MVOLMODEL) {
            switch (attr->mvolmode.left) {
            case SPU_VOICE_LINEARIncN:
                mvol_mode_left = SPU_VOL_MODE_LINEARIncN;
                break;
            case SPU_VOICE_LINEARIncR:
                mvol_mode_left = SPU_VOL_MODE_LINEARIncR;
                break;
            case SPU_VOICE_LINEARDecN:
                mvol_mode_left = SPU_VOL_MODE_LINEARDecN;
                break;
            case SPU_VOICE_LINEARDecR:
                mvol_mode_left = SPU_VOL_MODE_LINEARDecR;
                break;
            case SPU_VOICE_EXPIncN:
                mvol_mode_left = SPU_VOL_MODE_EXPIncN;
                break;
            case SPU_VOICE_EXPIncR:
                mvol_mode_left = SPU_VOL_MODE_EXPIncR;
                break;
            case SPU_VOICE_EXPDec:
                mvol_mode_left = SPU_VOL_MODE_EXPDec;
                break;
            case SPU_VOICE_DIRECT:
                vol_total_left = attr->mvol.left;
                mvol_mode_left = SPU_VOL_MODE_DIRECT;
                break;
            default:
                vol_total_left = attr->mvol.left;
                mvol_mode_left = SPU_VOL_MODE_DIRECT;
                break;
            }
        } else {
            vol_total_left = attr->mvol.left;
            mvol_mode_left = SPU_VOL_MODE_DIRECT;
        }

        if (mvol_mode_left != SPU_VOL_MODE_DIRECT) {
            if (attr->mvol.left > SPU_VOL_MAX) {
                vol_total_left = SPU_VOL_MAX;
            } else if (attr->mvol.left < 0) {
                vol_total_left = 0;
            } else {
                vol_total_left = attr->mvol.left;
            }
        }
        vol_total_left &= ~SPU_VOL_MODE_MASK;
        SPUW(main_vol.left, (vol_total_left) | mvol_mode_left);
    }

    if (bSetAll || mask & SPU_COMMON_MVOLR) {
        if (bSetAll || mask & SPU_COMMON_MVOLMODER) {
            switch (attr->mvolmode.right) {
            case SPU_VOICE_LINEARIncN:
                mvol_mode_right = SPU_VOL_MODE_LINEARIncN;
                break;
            case SPU_VOICE_LINEARIncR:
                mvol_mode_right = SPU_VOL_MODE_LINEARIncR;
                break;
            case SPU_VOICE_LINEARDecN:
                mvol_mode_right = SPU_VOL_MODE_LINEARDecN;
                break;
            case SPU_VOICE_LINEARDecR:
                mvol_mode_right = SPU_VOL_MODE_LINEARDecR;
                break;
            case SPU_VOICE_EXPIncN:
                mvol_mode_right = SPU_VOL_MODE_EXPIncN;
                break;
            case SPU_VOICE_EXPIncR:
                mvol_mode_right = SPU_VOL_MODE_EXPIncR;
                break;
            case SPU_VOICE_EXPDec:
                mvol_mode_right = SPU_VOL_MODE_EXPDec;
                break;
            case SPU_VOICE_DIRECT:
                vol_total_right = attr->mvol.right;
                mvol_mode_right = SPU_VOL_MODE_DIRECT;
                break;
            default:
                vol_total_right = attr->mvol.right;
                mvol_mode_right = SPU_VOL_MODE_DIRECT;
                break;
            }
        } else {
            vol_total_right = attr->mvol.right;
            mvol_mode_right = SPU_VOL_MODE_DIRECT;
        }

        if (mvol_mode_right != SPU_VOL_MODE_DIRECT) {
            if (attr->mvol.right > SPU_VOL_MAX) {
                vol_total_right = SPU_VOL_MAX;
            } else if (attr->mvol.right < 0) {
                vol_total_right = 0;
            } else {
                vol_total_right = attr->mvol.right;
            }
        }
        vol_total_right &= ~SPU_VOL_MODE_MASK;
        SPUW(main_vol.right, vol_total_right | mvol_mode_right);
    }

    if (bSetAll || (mask & SPU_COMMON_CDVOLL)) {
        SPUW(cd_vol.left, attr->cd.volume.left);
    }

    if (bSetAll || (mask & SPU_COMMON_CDVOLR)) {
        SPUW(cd_vol.right, attr->cd.volume.right);
    }

    if (bSetAll || (mask & SPU_COMMON_EXTVOLL)) {
        SPUW(ex_vol.left, attr->ext.volume.left);
    }

    if (bSetAll || (mask & SPU_COMMON_EXTVOLR)) {
        SPUW(ex_vol.right, attr->ext.volume.right);
    }

    if (bSetAll || (mask & SPU_COMMON_CDREV)) {
        if (attr->cd.reverb == 0) {
            cnt = SPUR(spucnt);
            cnt &= ~SPU_CTRL_MASK_CD_AUDIO_REVERB;
            SPUW(spucnt, cnt);
        } else {
            cnt = SPUR(spucnt);
            cnt |= SPU_CTRL_MASK_CD_AUDIO_REVERB;
            SPUW(spucnt, cnt);
        }
    }

    if (bSetAll || (mask & SPU_COMMON_CDMIX)) {
        if (attr->cd.mix == 0) {
            cnt = SPUR(spucnt);
            cnt &= ~SPU_CTRL_MASK_CD_AUDIO_ENABLE;
            SPUW(spucnt, cnt);
        } else {
            cnt = SPUR(spucnt);
            cnt |= SPU_CTRL_MASK_CD_AUDIO_ENABLE;
            SPUW(spucnt, cnt);
        }
    }

    if (bSetAll || (mask & SPU_COMMON_EXTREV)) {
        if (attr->ext.reverb == 0) {
            cnt = SPUR(spucnt);
            cnt &= ~SPU_CTRL_MASK_EXT_AUDIO_REVERB;
            SPUW(spucnt, cnt);
        } else {
            cnt = SPUR(spucnt);
            cnt |= SPU_CTRL_MASK_EXT_AUDIO_REVERB;
            SPUW(spucnt, cnt);
        }
    }

    if (bSetAll || (mask & SPU_COMMON_EXTMIX)) {
        if (attr->ext.mix == 0) {
            cnt = SPUR(spucnt);
            cnt &= ~SPU_CTRL_MASK_EXT_AUDIO_ENABLE;
            SPUW(spucnt, cnt);
        } else {
            cnt = SPUR(spucnt);
            cnt |= SPU_CTRL_MASK_EXT_AUDIO_ENABLE;
            SPUW(spucnt, cnt);
        }
    }
}
