#include "libspu_private.h"

void _SpuCallback(void (*cb)()) { InterruptCallback(9, cb); }
