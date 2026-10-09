#include "libsnd_private.h"

void (*SsSetTickCallback(void (*cb)(void)))(void) {
    void (*prev)(void);

    prev = _snd_seq_tick_env.tick_cb;
    if (cb != _snd_seq_tick_env.tick_cb) {
        _snd_seq_tick_env.tick_cb = cb;
    }
    return prev;
}
