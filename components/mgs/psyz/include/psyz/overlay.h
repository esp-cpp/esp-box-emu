#ifndef PSYZ_OVERLAY_H
#define PSYZ_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

// Generic overlay callbacks - platform-independent
// These work with any PSY-Z graphics backend.

// Called before the end of each frame, after the game has rendered.
// Use this to prepare overlay content (e.g., ImGui::NewFrame, ImGui::Render)
typedef void (*PsyzOverlayFrameCB)(void);

// Called when the graphics subsystem is shutting down.
// Use this to clean up overlay resources.
typedef void (*PsyzOverlayDestroyCB)(void);

// Register the frame start callback.
// Returns the previous callback, or NULL if none was set.
PsyzOverlayFrameCB Psyz_OverlayFrameCB(PsyzOverlayFrameCB cb);

// Register the destroy callback.
// Returns the previous callback, or NULL if none was set.
PsyzOverlayDestroyCB Psyz_OverlayDestroyCB(PsyzOverlayDestroyCB cb);

#ifdef __cplusplus
}
#endif

#endif // PSYZ_OVERLAY_H
