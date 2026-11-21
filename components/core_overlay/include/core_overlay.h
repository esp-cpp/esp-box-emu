#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The game cores' code overlay (overlay.ld): every core's code is linked at
/// one instruction-bus address and the core a game needs is copied there,
/// into PSRAM, when it launches; the other cores' code stays in flash.
///
/// Make a game core's code runnable: maps a PSRAM block the size of the
/// core's code at the overlay address, copies the core's code into it from
/// flash and, on the core's first load, runs its static constructors. Call it
/// before any of the core's code runs (before its cart is made) and only
/// while no other core's code runs; a core that is loaded already stays (any
/// other is unloaded first).
///
/// `core` is the overlay member's name: nes, gbc, sms, msx, genesis, doom,
/// darkforces or pico8. Returns false for any other name, or when the PSRAM
/// for the code cannot be had.
bool core_overlay_load(const char *core);

/// Unmap the loaded core's code and free its PSRAM (after its cart is gone:
/// nothing of the core may run any more, no task, callback or interrupt).
void core_overlay_unload(void);

/// The core whose code is loaded ("" for none).
const char *core_overlay_loaded(void);

#ifdef __cplusplus
}
#endif
