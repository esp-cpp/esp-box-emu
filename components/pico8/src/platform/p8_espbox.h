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

/* p8_update_input(): the player's buttons, BUTTON_MASK_* bits of p8_input.h */
uint16_t p8_espbox_buttons(void);

/* p8_pump_events() (the Lua instruction hook): blocks while the emulator menu
 * has the cart paused; unwinds the cart (p8_quit) when the emulator asked for
 * it to stop. */
void p8_espbox_pump(void);

#ifdef __cplusplus
}
#endif
