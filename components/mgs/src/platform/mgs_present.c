/* Frame presentation for Metal Gear Solid on esp-box-emu.
 *
 * The port's scanout task (esp32_vblank.c) calls lcd_present() with a pointer
 * into PSX VRAM (1024 halfwords per row) whenever the game has flipped a frame.
 * The 320x240 display area is converted from the PSX 15-bit BGR (red in the low
 * bits) to RGB565 into one of two display buffers and handed to the emulator's
 * video task, which double-buffers against the display itself.
 */
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mgs_glue.h"

#define MGS_W 320
#define MGS_H 240
#define VRAM_STRIDE 1024

uint16_t* mgs_present_buffers[2];
volatile unsigned mgs_presented_frames;

void lcd_present(const uint16_t* src) {
    static int index;
    uint16_t* dst = mgs_present_buffers[index];
    if (!dst) {
        return;
    }
    index ^= 1;
    for (int y = 0; y < MGS_H; y++) {
        const uint16_t* row = src + y * VRAM_STRIDE;
        uint16_t* out = dst + y * MGS_W;
        for (int x = 0; x < MGS_W; x += 2) {
            const uint32_t c0 = row[x];
            const uint32_t c1 = row[x + 1];
            out[x]     = (uint16_t)(((c0 & 0x1F) << 11) | (((c0 >> 5) & 0x1F) << 6) | ((c0 >> 10) & 0x1F));
            out[x + 1] = (uint16_t)(((c1 & 0x1F) << 11) | (((c1 >> 5) & 0x1F) << 6) | ((c1 >> 10) & 0x1F));
        }
    }
    mgs_presented_frames++;
    mgs_present_frame(dst);
}

/* The upstream boards share one SPI bus between the panel and the card; the
 * box has separate buses and the emulator's own display path. */
void Mgs_SpiBusTake(void) {}
void Mgs_SpiBusGive(void) {}

/* the mts idle task's poll (libsio/dummy.c): give core 0 to the emulator */
void Mgs_IdleYield(void) { vTaskDelay(1); }
