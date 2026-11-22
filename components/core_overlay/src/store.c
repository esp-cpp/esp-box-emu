// The flash store of the game cores' code overlay (overlay.ld): a header and
// every core's linked code, written into this array after linking by
// tools/core_overlay.py image. Its size (core_ovl_store_size.h) is computed
// before the link from the cores' archives (an upper bound: the link drops
// unused code). In .flash.rodata, so the image's SHA-256 covers it.
#include <stdint.h>

#include "core_ovl_store_size.h"

__attribute__((section(".rodata.core_ovl_store"), aligned(16))) const uint8_t core_ovl_store[CORE_OVL_STORE_SIZE] = {0};
