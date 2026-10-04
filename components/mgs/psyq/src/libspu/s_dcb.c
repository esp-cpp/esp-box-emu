#include <common.h>
#include <libspu.h>
#include <libetc.h>

void _SpuDataCallback(void (*arg0)()) { DMACallback(4, arg0); }
