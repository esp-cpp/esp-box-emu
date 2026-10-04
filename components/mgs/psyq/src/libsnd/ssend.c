#include "libsnd_private.h"
#include "kernel.h"
#include "libetc.h"

void SsEnd(void) {
    if (_snd_seq_tick_env.manual_tick == 0) {
        _snd_seq_tick_env.unk11 = 0;
        if (_snd_seq_tick_env.alarm_tick == 0x7F) {
            return;
        }
        EnterCriticalSection();
        if (_snd_seq_tick_env.vsync_tick) {
            VSyncCallback(NULL);
            _snd_seq_tick_env.vsync_tick = 0;
        } else if (_snd_seq_tick_env.alarm_tick == 0) {
            InterruptCallback(0, _snd_seq_tick_env.vsync_cb);
            _snd_seq_tick_env.vsync_cb = NULL;
        } else {
            InterruptCallback(6, NULL);
        }
        ExitCriticalSection();
        _snd_seq_tick_env.alarm_tick = 0x7F;
    }
}
