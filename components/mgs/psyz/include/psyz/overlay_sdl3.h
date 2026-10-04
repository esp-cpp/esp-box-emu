#ifndef PSYZ_OVERLAY_SDL3_H
#define PSYZ_OVERLAY_SDL3_H
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called for each SDL3 input event.
// Use this to forward events to overlay input handling (e.g., ImGui).
typedef void (*PsyzOverlayEventCB_SDL3)(const SDL_Event* event);

// Register the SDL3 event callback.
// Returns the previous callback, or NULL if none was set.
PsyzOverlayEventCB_SDL3 Psyz_OverlayEvent_SDL3(PsyzOverlayEventCB_SDL3 cb);

#ifdef __cplusplus
}
#endif

#endif // PSYZ_OVERLAY_SDL3_H
