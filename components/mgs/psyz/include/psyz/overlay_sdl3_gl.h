#ifndef PSYZ_OVERLAY_SDL3_GL_H
#define PSYZ_OVERLAY_SDL3_GL_H
#include <SDL3/SDL.h>
#include <psyz/overlay_sdl3.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called once after SDL3/OpenGL initialization completes.
// Use this to initialize overlay backends (e.g., ImGui_ImplSDL3_InitForOpenGL).
typedef void (*PsyzOverlayInitCB_SDL3GL)(SDL_Window* window, SDL_GLContext ctx);

// Register the SDL3+OpenGL initialization callback.
// Returns the previous callback, or NULL if none was set.
PsyzOverlayInitCB_SDL3GL Psyz_OverlayInit_SDL3GL(PsyzOverlayInitCB_SDL3GL cb);

#ifdef __cplusplus
}
#endif

#endif // PSYZ_OVERLAY_SDL3_GL_H
