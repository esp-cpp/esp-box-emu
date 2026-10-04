#pragma once
// esp-box-emu glue shared between the C port layer and src/mgs.cpp.
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sizes of the game's big static buffers, which live in the emulator's 4MB ROM
// block while the game runs (see src/mgs.cpp).
#define MGS_VRAM_BYTES        (1024u * 512u * 2u)             /* PSX VRAM, 16bpp */
#define MGS_MAIN_RAM_BYTES    (0x80000u + 0xC9000u + 0x20000u) /* libgv.h: headroom + RAM map + top */
#define MGS_FACE_GROUP_BYTES  (0x80000u)                      /* psyz_port.c */

// Pad state written by the emulator loop (PSX bit layout, active high) and
// turned into the pad frames the game parses by the vblank tick.
extern volatile uint16_t mgs_box_pad;

// Display buffers (RGB565, 320x240) the presenter converts VRAM into; set by
// src/mgs.cpp before the game starts, cleared at shutdown.
extern uint16_t* mgs_present_buffers[2];
// Called by the presenter with the converted frame.
void mgs_present_frame(const uint16_t* frame);
// Frames presented so far (for the FPS statistics).
extern volatile unsigned mgs_presented_frames;

// Port layer (mgs/port) entry points used by the glue.
void Mgs_SetDataRoot(const char* path);
void Mgs_CdInit(void);
void Mgs_CdDeinit(void);
void Mgs_StartVblank(void);
void Mgs_PauseVblank(void);
void Mgs_ResumeVblank(void);
void Mgs_StopVblank(void);
void Mgs_ThreadsPause(void);
void Mgs_ThreadsResume(void);
void Mgs_ThreadsStopAll(void);
int mgs_main(void);

#ifdef __cplusplus
}
#endif
