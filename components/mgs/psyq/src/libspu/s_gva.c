#include "libspu_private.h"

/* declared by <libspu.h>; the local copy used s32 where the header uses int,
 * which are distinct types on a target where int32_t is long */
void SpuGetVoiceAttr(SpuVoiceAttr* attr) {
    s32 voice;
    s32 i;

    voice = -1;
    for (i = 0; i < NUM_VOICES; i++) {
        if (attr->voice & (1 << i)) {
            voice = i;
            break;
        }
    }

    if (voice != -1) {
        SpuNGetVoiceAttr(voice, attr);
    }
}
