#include "libapi.h"
#include "libsnd_private.h"
#include <kernel.h>
#include <libapi.h>
#include <libetc.h>

void _SsTrapIntrVSync(void);
void _SsSeqCalledTbyT_1per2(void);

struct SndSeqTickEnv _snd_seq_tick_env = {
    60, 1, SsSeqCalledTbyT, NULL, 0, 0, 127};

void _SsStart(int use_vsync) {
    unsigned short rcnt_target;
    int wait;
    int rcnt_spec;
    u8 alarm_tick;
    void (*cb)(void);
    void (*prevCb)(void);

    struct SndSeqTickEnv* env = &_snd_seq_tick_env;

    wait = 999;
    do {
        wait--;
    } while (wait >= 0);
    _snd_seq_tick_env.vsync_tick = 0;
    _snd_seq_tick_env.alarm_tick = 6;
    _snd_seq_tick_env.unk11 = 0;
    _snd_seq_tick_env.vsync_cb = 0;
    rcnt_spec = 0xF2000002;
    rcnt_target = 17640;
    switch (_snd_seq_tick_env.tick_mode) {
    case SS_NOTICK0:
        _snd_seq_tick_env.alarm_tick = 127;
        return;
    case SS_TICKVSYNC:
        _snd_seq_tick_env.alarm_tick = 0;
        if (use_vsync == 0) {
            _snd_seq_tick_env.vsync_tick = 1;
        } else {
            rcnt_spec = 0xF2000003;
            rcnt_target = 1;
        }
        break;
    case SS_TICK120:
        rcnt_target = 35280;
        break;
    case SS_TICK240:
        break;
    default:
        if (_snd_seq_tick_env.manual_tick) {
            return;
        }
        if (_snd_seq_tick_env.tick_mode < 70) {
            rcnt_target = 2116800 / _snd_seq_tick_env.tick_mode;
            _snd_seq_tick_env.unk11++;
        } else {
            rcnt_target = 4233600 / _snd_seq_tick_env.tick_mode;
        }
        break;
    }

    if (_snd_seq_tick_env.vsync_tick) {
        EnterCriticalSection();
        VSyncCallback(_snd_seq_tick_env.tick_cb);
    } else {
        EnterCriticalSection();
        ResetRCnt(rcnt_spec);
        SetRCnt(rcnt_spec, rcnt_target, 0x1000);
        alarm_tick = _snd_seq_tick_env.alarm_tick;
        if (alarm_tick == 0) {
            prevCb = InterruptCallback(0, 0);
            cb = _SsTrapIntrVSync;
            alarm_tick = _snd_seq_tick_env.alarm_tick;
            _snd_seq_tick_env.vsync_cb = prevCb;
        } else {
            cb = _SsSeqCalledTbyT_1per2;
            if (_snd_seq_tick_env.unk11 == 0) {
                cb = _snd_seq_tick_env.tick_cb;
            }
        }
        InterruptCallback(alarm_tick, cb);
    }
    ExitCriticalSection();
}

void SsStart(void) { _SsStart(1); }

void SsStart2(void) { _SsStart(0); }

void _SsTrapIntrVSync(void) {
    struct SndSeqTickEnv* env = &_snd_seq_tick_env;

    if (env->vsync_cb) {
        env->vsync_cb();
    }
    env->tick_cb();
}

void _SsSeqCalledTbyT_1per2(void) {
    struct SndSeqTickEnv* env = &_snd_seq_tick_env;

    if (env->unk14 == 0) {
        env->unk14 = 1;
    } else {
        env->unk14 = 0;
        env->tick_cb();
    }
}
