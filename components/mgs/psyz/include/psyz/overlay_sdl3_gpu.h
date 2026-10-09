#ifndef PSYZ_OVERLAY_SDL3_GPU_H
#define PSYZ_OVERLAY_SDL3_GPU_H
#include <SDL3/SDL.h>
#include <psyz/overlay_sdl3.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called once after SDL3/SDL_GPU initialization completes.
// Use this to initialize overlay backends (e.g., ImGui_ImplSDLGPU3_Init).
// Query swapchain texture format with SDL_GetGPUSwapchainTextureFormat.
typedef void (*PsyzOverlayInitCB_SDL3GPU)(
    SDL_Window* window, SDL_GPUDevice* device);

// Called each presented frame inside a render pass targeting the swapchain
// texture, after the PS1 framebuffer has been blitted to it.
// Use with ImGui_ImplSDLGPU3_RenderDrawData.
typedef void (*PsyzOverlayRenderCB_SDL3GPU)(
    SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass);

// Register the SDL3+SDL_GPU initialization callback.
// Returns the previous callback, or NULL if none was set.
PsyzOverlayInitCB_SDL3GPU Psyz_OverlayInit_SDL3GPU(
    PsyzOverlayInitCB_SDL3GPU cb);

// Register the SDL3+SDL_GPU render callback.
// Returns the previous callback, or NULL if none was set.
PsyzOverlayRenderCB_SDL3GPU Psyz_OverlayRender_SDL3GPU(
    PsyzOverlayRenderCB_SDL3GPU cb);

#ifdef __cplusplus
}
#endif

#endif // PSYZ_OVERLAY_SDL3_GPU_H
