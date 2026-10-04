#include <common.h>
#include <libsnd.h>
#include <libspu.h>
#include <libetc.h>

void _SsInit(void);
void SsInit(void) {
    ResetCallback();
    SpuInit();
    _SsInit();
}
