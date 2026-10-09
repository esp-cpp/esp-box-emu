/* The software GPU backend for the portable Metal Gear Solid build.
 *
 * This compiles the SAME portable core the SOTN port and the PC harness use
 * (soft_raster.inc.c, standalone flavour: it brings its own GP0 packet parser
 * and the Draw_* state machine), so a divergence between desktop and board is
 * always the platform layer and never the raster maths.
 *
 * VRAM is the PSX's 1 MB frame buffer as 512 rows of 1024 RGB5551 halfwords.
 * On the ESP32-S3 this wants EXT_RAM_BSS_ATTR to land in PSRAM; that attribute
 * is applied by the ESP-IDF build, not here, so this file stays target-neutral.
 */

#include <psyz.h>
#include <libgpu.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "internal.h"
#include "draw.h"

/* The frame buffer the rasterizer writes into: 512 x 1024 halfwords = 1 MB.
 * Far too big for the S3's internal SRAM, so it goes to PSRAM. Needs
 * CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY. */
#if defined(MGS_ESPBOX)
#include "esp_attr.h"
u16* g_RawVram;   /* esp-box-emu: from the ROM block, VRAM_W * VRAM_H halfwords */
#elif defined(ESP_PLATFORM)
#include "esp_attr.h"
EXT_RAM_BSS_ATTR u16 g_RawVram[VRAM_H * VRAM_W];
#else
u16 g_RawVram[VRAM_H * VRAM_W];
#endif

#define WARNF(fmt, ...) printf("[soft] " fmt "\n", ##__VA_ARGS__)
#define SOFT_RASTER_STANDALONE
#define SOFT_RASTER_NO_FALLBACK
#ifndef SOFT_RASTER_IRAM
/* The per-pixel loops must not execute from flash. Left empty they run XIP
 * through the instruction cache, and a triangle filler is exactly the shape
 * that thrashes it: a tight loop competing with the texel and framebuffer
 * traffic it is generating. The SOTN port on this same board sets IRAM_ATTR
 * here and reaches 60 fps; MGS had it empty. */
#define SOFT_RASTER_IRAM IRAM_ATTR
#endif
#define SOFT_CLUT_CACHE
#include "platform/soft_raster.inc.c"

/* The scanout asks which rows of VRAM the drawing area covers, so it only
 * copies what the game actually draws. */
void Soft_VisibleRows(int* y0, int* y1) {
    *y0 = st.y0;
    *y1 = st.y1;
}

/* Where scanout should read from. MGS drives the display through libdg's own
 * double buffer, so this is filled in when the display path is wired up; until
 * then the top-left of VRAM is a safe answer. */
void Mgs_DisplayOrigin(int* x, int* y) {
    if (x) *x = 0;
    if (y) *y = 0;
}

/* Bind the rasterizer to VRAM and open the drawing area to the whole frame
 * buffer.
 *
 * psyz calls this from its own ResetGraph, but MGS goes through the PSY-Q
 * decomp's ResetGraph instead, which never reaches it -- so the first
 * ClearImage faulted on a NULL `vram` (EXCVADDR 0x00000000, StoreProhibited).
 * Bind at constructor time as well, exactly as the SOTN port had to. */
__attribute__((constructor)) static void bind_vram_early(void) {
    void Draw_Reset(void);
    Draw_Reset();
}

void Draw_Reset(void) {
    vram = g_RawVram;
    st.x0 = 0;
    st.y0 = 0;
    st.x1 = VRAM_W - 1;
    st.y1 = VRAM_H - 1;
    st.ox = st.oy = 0;
}

/* Frame telemetry psyz's libgpu.c references (SOTN defines these in its own
 * backend shell). Readable over the console to see what the GPU stream did. */
unsigned sotn_prim_cycles;
unsigned sotn_prim_census[8];
unsigned sotn_prim_cycles_by[8];
unsigned sotn_poly_kind[4];
unsigned sotn_move_count;
unsigned sotn_exeque_count;
unsigned sotn_push_count;
unsigned sotn_pkt_drops;
