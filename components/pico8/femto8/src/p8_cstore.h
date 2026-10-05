#ifndef P8_CSTORE_H
#define P8_CSTORE_H

#include <stdint.h>

int write_cart_p8(const char *path, const char *lua_script, const uint8_t *memory);

#endif
