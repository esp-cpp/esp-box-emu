/**
 * Copyright (C) 2026 Chris January, Ben Baker, and contributors
 */

#ifndef P8_INPUT_H
#define P8_INPUT_H

#include "p8_emu.h"

#if defined(SDL)
#include "SDL.h"
#elif defined(__DA1470x__)
#include "gdi.h"
#endif

#ifdef SDL
#define INPUT_LEFT SDLK_LEFT
#define INPUT_RIGHT SDLK_RIGHT
#define INPUT_UP SDLK_UP
#define INPUT_DOWN SDLK_DOWN
#define INPUT_ACTION1 SDLK_z
#define INPUT_ACTION2 SDLK_x
#define INPUT_ESCAPE SDLK_ESCAPE
#define NUM_SCANCODES 512
#else
#define NUM_SCANCODES 256
#endif

enum
{
    BUTTON_LEFT = 0,
    BUTTON_RIGHT = 1,
    BUTTON_UP = 2,
    BUTTON_DOWN = 3,
    BUTTON_ACTION1 = 4,
    BUTTON_ACTION2 = 5,
    BUTTON_PAUSE = 6,
    BUTTON_ESCAPE = 8,
    BUTTON_RETURN = 9,
    BUTTON_SPACE = 10,
    BUTTON_PAGE_UP = 11,
    BUTTON_PAGE_DOWN = 12,
};

enum
{
    BUTTON_MASK_LEFT      = (1 << BUTTON_LEFT),
    BUTTON_MASK_RIGHT     = (1 << BUTTON_RIGHT),
    BUTTON_MASK_UP        = (1 << BUTTON_UP),
    BUTTON_MASK_DOWN      = (1 << BUTTON_DOWN),
    BUTTON_MASK_ACTION1   = (1 << BUTTON_ACTION1),
    BUTTON_MASK_ACTION2   = (1 << BUTTON_ACTION2),
    BUTTON_MASK_PAUSE     = (1 << BUTTON_PAUSE),
    BUTTON_MASK_ESCAPE    = (1 << BUTTON_ESCAPE),
    BUTTON_MASK_RETURN    = (1 << BUTTON_RETURN),
    BUTTON_MASK_SPACE     = (1 << BUTTON_SPACE),
    BUTTON_MASK_PAGE_UP   = (1 << BUTTON_PAGE_UP),
    BUTTON_MASK_PAGE_DOWN = (1 << BUTTON_PAGE_DOWN),
};

#ifndef SDL
#define KMOD_LSHIFT      0x000001 // shift, caps shift
#define KMOD_RSHIFT      0x000002
#define KMOD_LCTRL       0x000040
#define KMOD_RCTRL       0x000080
#define KMOD_LALT        0x000100 // alt, option, symbol shift
#define KMOD_RALT        0x000200
#define KMOD_LMETA       0x000400 // Windows, command, meta
#define KMOD_RMETA       0x000800
#define KMOD_NUM_LOCK    0x001000
#define KMOD_CAPS_LOCK   0x002000
#define KMOD_SCROLL_LOCK 0x008000

#define KMOD_SHIFT (KMOD_LSHIFT | KMOD_RSHIFT)
#define KMOD_CTRL  (KMOD_LCTRL  | KMOD_RCTRL)
#define KMOD_ALT   (KMOD_LALT   | KMOD_RALT)
#define KMOD_META  (KMOD_LMETA  | KMOD_RMETA)
#endif

extern int16_t m_mouse_x, m_mouse_y;
extern int16_t m_mouse_xrel, m_mouse_yrel;
extern uint8_t m_mouse_buttons, m_mouse_buttonsp;
extern int8_t m_mouse_wheel;
extern unsigned m_mouse_keymod;
#ifndef NEXTP8
extern bool m_scancodes[NUM_SCANCODES];
#endif

extern uint16_t m_buttons[PLAYER_COUNT];
extern uint16_t m_buttonsp[PLAYER_COUNT];

bool p8_get_next_keypress(unsigned *scancode, uint8_t *keychar, unsigned *mod);
bool p8_get_next_mouse_click(int *x, int *y, int *button, unsigned *mod);
bool p8_has_pending_keypress(void);
bool p8_is_key_down(unsigned scancode);
void p8_init_input(void);
void p8_pump_events(void);
void p8_reset_input(void);
void p8_update_input(void);

#endif
