#include <common.h>
#include <libetc.h>
#include <libspu.h>
#include "libsnd_private.h"

void SsInitHot(void) {
    ResetCallback();
    SpuInitHot();
    _SsInit();
}
