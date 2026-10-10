#pragma once
/* esp-box-emu: save states for femto8 (see p8_state.c) */
#include "lua.h"

#ifdef __cplusplus
extern "C" {
#endif

/* after the API is registered and before the cart runs: records the
 * permanent objects (API functions, library tables) by name */
void p8_state_init(lua_State *L);
/* only between frames (no cart code on the Lua stack); 0 on success */
int p8_state_save(const char *path);
int p8_state_load(const char *path);

#ifdef __cplusplus
}
#endif
