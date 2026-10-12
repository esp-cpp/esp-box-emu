#pragma once
/* esp-box-emu platform for femto8 (FEMTO8_ESPBOX): the emulator supplies
 * video, audio and input. Implemented in src/pico8.cpp. */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* p8_render(): a 128x128 buffer of 5-bit colour indices (into m_colors) to
 * draw the frame into, or NULL to skip the frame; frame_end() presents it. */
uint8_t* p8_espbox_frame_begin(void);
void p8_espbox_frame_end(void);

/* p8_main_loop(), before _draw: non-zero when the display can take a new frame.
 * The LCD manages ~31-43 fps, so a 60 fps cart would have every other frame
 * discarded after _draw and the render had been paid for; when this returns 0
 * the loop skips _draw (as PICO-8 itself does under load) and only paces. */
int p8_espbox_display_ready(void);

/* p8_update_input(): the player's buttons, BUTTON_MASK_* bits of p8_input.h */
uint16_t p8_espbox_buttons(void);

/* p8_pump_events() (the Lua instruction hook): blocks while the emulator menu
 * has the cart paused; unwinds the cart (p8_quit) when the emulator asked for
 * it to stop. */
void p8_espbox_pump(void);

/* p8_main_loop(), between frames: the Lua stack holds no cart code, so this
 * is where a save/load state requested from the emulator menu is done. */
void p8_espbox_frame_boundary(void);

#ifdef __cplusplus
}
#endif
