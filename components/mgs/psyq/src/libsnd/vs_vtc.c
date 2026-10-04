#include "libsnd_private.h"

short SsVabTransCompleted(short immediateFlag) {
    return SpuIsTransferCompleted(immediateFlag);
}
