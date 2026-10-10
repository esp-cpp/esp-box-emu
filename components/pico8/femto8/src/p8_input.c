/**
 * Copyright (C) 2026 Chris January, Ben Baker, and contributors
 */

#include <string.h>

#include "p8_emu.h"
#include "p8_input.h"

#if defined(SDL)
#include "SDL.h"
#elif defined(__DA1470x__)
#include "gdi.h"
#elif defined(FEMTO8_ESPBOX)
#include "p8_espbox.h"
#endif

#ifdef SDL
/* Expose window from p8_emu for mouse grab/relative mode changes. */
extern SDL_Window *m_window;
#endif

#define DEFAULT_AUTO_REPEAT_DELAY_MS (DEFAULT_AUTO_REPEAT_DELAY * 1000 / 30)
#define DEFAULT_AUTO_REPEAT_INTERVAL_MS (DEFAULT_AUTO_REPEAT_INTERVAL * 1000 / 30)

int16_t m_mouse_x, m_mouse_y;
int16_t mouse_x4, mouse_y4;
int16_t m_mouse_xrel, m_mouse_yrel;
uint8_t m_mouse_buttons, m_mouse_buttonsp;
int8_t m_mouse_wheel;
unsigned m_mouse_keymod;
static uint8_t m_mouse_click_buttons;
static int16_t m_mouse_click_x;
static int16_t m_mouse_click_y;
static unsigned m_mouse_click_mod;

#define MAX_KEY_EVENTS 8

struct key_event {
    unsigned scancode;
    unsigned keymod;
    uint8_t ch;
};

static int m_key_queue_write_index = 0;
static int m_key_queue_read_index  = 0;
static struct key_event m_key_queue_buffer[MAX_KEY_EVENTS];
bool m_scancodes[NUM_SCANCODES];

uint16_t m_buttons[PLAYER_COUNT];
uint16_t m_buttonsp[PLAYER_COUNT];
static uint16_t m_button_first_repeat[PLAYER_COUNT];
static unsigned m_button_down_time[PLAYER_COUNT][BUTTON_INTERNAL_COUNT];
#ifdef SDL
static uint16_t m_buttons_latch[PLAYER_COUNT];
#endif

static bool m_prev_pointer_lock;

#ifdef SDL
static void update_buttons(int index, int button, bool state)
{
    uint16_t mask = m_buttons[index];
    mask = state ? mask | (1 << button) : mask & ~(1 << button);
    m_buttons[index] = mask;
    m_memory[MEMORY_BUTTON_STATE + index] = mask & 0xff;
    if (state)
        m_buttons_latch[index] |= (1 << button);
}
#endif

bool p8_is_key_down(unsigned scancode)
{
    if (scancode >= NUM_SCANCODES)
        return false;
    return m_scancodes[scancode];
}

static bool is_modifier(unsigned sdl2_sc)
{
    switch (sdl2_sc) {
        case 225:
        case 229:
        case 224:
        case 228:
        case 226:
        case 230:
        case 227:
        case 231:
            return true;
        default:
            return false;
    }
}

static void clear_input_queue(void)
{
    m_key_queue_read_index = m_key_queue_write_index;
    m_mouse_click_buttons = 0;
    m_mouse_click_x = 0;
    m_mouse_click_y = 0;
    m_mouse_click_mod = 0;
}

static void queue_keypress(unsigned scancode, uint8_t keychar, unsigned mod)
{
    if (is_modifier(scancode))
        return;
    if ((m_key_queue_write_index + 1) % MAX_KEY_EVENTS == m_key_queue_read_index)
    {
       // buffer is full, avoid overflow
       return;
    }
    m_key_queue_buffer[m_key_queue_write_index].scancode = scancode;
    m_key_queue_buffer[m_key_queue_write_index].ch = keychar;
    m_key_queue_buffer[m_key_queue_write_index].keymod = mod;
    m_key_queue_write_index = (m_key_queue_write_index + 1) % MAX_KEY_EVENTS;
}

static void queue_mouse_click(int buttons, int x, int y, unsigned mod)
{
    m_mouse_click_buttons = buttons;
    m_mouse_click_x = x;
    m_mouse_click_y = y;
    m_mouse_click_mod = mod;
}

bool p8_get_next_keypress(unsigned *scancode, uint8_t *keychar, unsigned *mod)
{
    if (m_key_queue_read_index == m_key_queue_write_index) {
       // buffer is empty
       return false;
    }

    *scancode = m_key_queue_buffer[m_key_queue_read_index].scancode;
    *keychar = m_key_queue_buffer[m_key_queue_read_index].ch;
    *mod = m_key_queue_buffer[m_key_queue_read_index].keymod;
    m_key_queue_read_index = (m_key_queue_read_index + 1) % MAX_KEY_EVENTS;
    return true;
}

bool p8_get_next_mouse_click(int *x, int *y, int *button, unsigned *mod)
{
    bool has_click = (m_mouse_click_buttons != 0);

    if (x) *x = m_mouse_click_x;
    if (y) *y = m_mouse_click_y;
    if (button) *button = m_mouse_click_buttons;

    m_mouse_click_x = 0;
    m_mouse_click_y = 0;
    m_mouse_click_buttons = 0;

    return has_click;
}

bool p8_has_pending_keypress(void)
{
    return m_key_queue_read_index != m_key_queue_write_index;
}

void p8_init_input(void)
{
    p8_reset_input();
}

void p8_pump_events(void)
{
#if defined(FEMTO8_ESPBOX)
    /* called from the Lua instruction hook: the VM is at a safe point, so
     * this is where the emulator menu pauses the cart and where a quit
     * requested from outside unwinds it */
    p8_espbox_pump();
#elif defined(SDL)
    SDL_PumpEvents();

    // SDL_QUIT must be consumed and acted on immediately.
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUIT, SDL_QUIT) > 0)
        p8_abort();

    // Consume all pending keydown/keyup events.  Handle pause/escape immediately,
    // and re-queue everything else so p8_update_input processes it normally.
    SDL_Event keyevents[64];
    int n = SDL_PeepEvents(keyevents, 64, SDL_GETEVENT, SDL_KEYDOWN, SDL_KEYUP);
    for (int i = 0; i < n; i++) {
        SDL_Keycode sym = keyevents[i].key.keysym.sym;
        bool is_pause = (sym == SDLK_RETURN || sym == SDLK_p);
        bool is_escape = (sym == INPUT_ESCAPE);
        if (is_pause || is_escape) {
            if (sym == SDLK_RETURN) {
                if (keyevents[i].type == SDL_KEYDOWN) {
                    if ((m_buttons[0] & BUTTON_MASK_PAUSE) == 0)
                        m_buttonsp[0] |= BUTTON_MASK_PAUSE;
                    if ((m_buttons[0] & BUTTON_MASK_RETURN) == 0)
                        m_buttonsp[0] |= BUTTON_MASK_RETURN;
                    m_buttons[0] |= BUTTON_MASK_PAUSE | BUTTON_MASK_RETURN;
                    m_button_down_time[0][BUTTON_PAUSE] = UINT_MAX;
                    m_button_down_time[0][BUTTON_RETURN] = UINT_MAX;
                } else {
                    m_buttons[0] &= ~(BUTTON_MASK_PAUSE | BUTTON_MASK_RETURN);
                }
            } else if (sym == SDLK_p) {
                if (keyevents[i].type == SDL_KEYDOWN) {
                    if ((m_buttons[0] & BUTTON_MASK_PAUSE) == 0)
                        m_buttonsp[0] |= BUTTON_MASK_PAUSE;
                    m_buttons[0] |= BUTTON_MASK_PAUSE;
                    m_button_down_time[0][BUTTON_PAUSE] = UINT_MAX;
                } else {
                    m_buttons[0] &= ~BUTTON_MASK_PAUSE;
                }
            } else if (sym == INPUT_ESCAPE) {
                if (keyevents[i].type == SDL_KEYDOWN) {
                    if ((m_buttons[0] & BUTTON_MASK_ESCAPE) == 0)
                        m_buttonsp[0] |= BUTTON_MASK_ESCAPE;
                    m_buttons[0] |= BUTTON_MASK_ESCAPE;
                    m_button_down_time[0][BUTTON_ESCAPE] = UINT_MAX;
                } else {
                    m_buttons[0] &= ~BUTTON_MASK_ESCAPE;
                }
            }
        } else {
            SDL_PushEvent(&keyevents[i]);
        }
    }
#endif
}

void p8_reset_input()
{
    for (unsigned p=0;p<2;++p) {
        for (unsigned i=0;i<BUTTON_INTERNAL_COUNT;++i) {
            m_button_down_time[p][i] = UINT_MAX;
        }
        m_button_first_repeat[p] = 1;
    }
}

void p8_update_input()
{
    bool pointer_lock = (m_memory[MEMORY_DEVKIT_MODE] & 0x4) != 0;
    if (pointer_lock != m_prev_pointer_lock) {
        m_prev_pointer_lock  = pointer_lock;
#ifdef SDL
        SDL_SetRelativeMouseMode(pointer_lock ? SDL_TRUE : SDL_FALSE);
        if (m_window)
            SDL_SetWindowGrab(m_window, pointer_lock ? SDL_TRUE : SDL_FALSE);
#endif
    }

    clear_input_queue();

#ifdef SDL
    m_mouse_xrel = 0;
    m_mouse_yrel = 0;
    m_mouse_wheel = 0;
    m_mouse_buttonsp = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_MOUSEMOTION:
            m_mouse_x = event.motion.x * P8_WIDTH / SCREEN_WIDTH;
            m_mouse_y = event.motion.y * P8_HEIGHT / SCREEN_HEIGHT;
            m_mouse_xrel += event.motion.xrel * P8_WIDTH / SCREEN_WIDTH;
            m_mouse_yrel += event.motion.yrel * P8_HEIGHT / SCREEN_HEIGHT;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == 1) {
                if (!(m_mouse_buttons & 0x1))
                    m_mouse_buttonsp |= 0x1;
                m_mouse_buttons |= 0x1;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_ACTION1, true);
            } else if (event.button.button == 3) {
                if (!(m_mouse_buttons & 0x2))
                    m_mouse_buttonsp |= 0x2;
                m_mouse_buttons |= 0x2;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_ACTION2, true);
            } else if (event.button.button == 2) {
                if (!(m_mouse_buttons & 0x4))
                    m_mouse_buttonsp |= 0x4;
                m_mouse_buttons |= 0x4;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_PAUSE, true);
            } else if (event.button.button == 4) {
                m_mouse_wheel += 1;
            } else if (event.button.button == 5) {
                m_mouse_wheel -= 1;
            }
            queue_mouse_click(event.button.button, event.button.x, event.button.y, m_mouse_keymod);
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == 1) {
                m_mouse_buttons &= ~0x1;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_ACTION1, false);
            } else if (event.button.button == 3) {
                m_mouse_buttons &= ~0x2;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_ACTION2, false);
            } else if (event.button.button == 2) {
                m_mouse_buttons &= ~0x4;
                if (m_memory[MEMORY_DEVKIT_MODE] & 0x2)
                    update_buttons(0, BUTTON_PAUSE, false);
            }
            break;
        case SDL_TEXTINPUT:
            if (event.text.text[0] != '\0') {
                queue_keypress(0, (uint8_t)event.text.text[0], 0);
            }
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case INPUT_LEFT:
                update_buttons(0, BUTTON_LEFT, true);
                break;
            case INPUT_RIGHT:
                update_buttons(0, BUTTON_RIGHT, true);
                break;
            case INPUT_UP:
                update_buttons(0, BUTTON_UP, true);
                break;
            case INPUT_DOWN:
                update_buttons(0, BUTTON_DOWN, true);
                break;
            case INPUT_ACTION1:
                update_buttons(0, BUTTON_ACTION1, true);
                break;
            case INPUT_ACTION2:
                update_buttons(0, BUTTON_ACTION2, true);
                break;
            case INPUT_ESCAPE:
                update_buttons(0, BUTTON_ESCAPE, true);
                break;
            case SDLK_RETURN:
                update_buttons(0, BUTTON_PAUSE, true);
                update_buttons(0, BUTTON_RETURN, true);
                break;
            case SDLK_p:
                update_buttons(0, BUTTON_PAUSE, true);
                break;
            case SDLK_SPACE:
                update_buttons(0, BUTTON_SPACE, true);
                break;
            case SDLK_PAGEUP:
                update_buttons(0, BUTTON_PAGE_UP, true);
                break;
            case SDLK_PAGEDOWN:
                update_buttons(0, BUTTON_PAGE_DOWN, true);
                break;
            default:
                break;
            }
            {
                if (event.key.keysym.scancode > 0 && event.key.keysym.scancode < NUM_SCANCODES)
                    m_scancodes[event.key.keysym.scancode] = true;
                queue_keypress(event.key.keysym.scancode, event.key.keysym.sym < 32 ? event.key.keysym.sym : 0, event.key.keysym.mod);
                m_mouse_keymod = event.key.keysym.mod;
            }
            break;
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
            case INPUT_LEFT:
                update_buttons(0, BUTTON_LEFT, false);
                break;
            case INPUT_RIGHT:
                update_buttons(0, BUTTON_RIGHT, false);
                break;
            case INPUT_UP:
                update_buttons(0, BUTTON_UP, false);
                break;
            case INPUT_DOWN:
                update_buttons(0, BUTTON_DOWN, false);
                break;
            case INPUT_ACTION1:
                update_buttons(0, BUTTON_ACTION1, false);
                break;
            case INPUT_ACTION2:
                update_buttons(0, BUTTON_ACTION2, false);
                break;
            case SDLK_RETURN:
                update_buttons(0, BUTTON_PAUSE, false);
                update_buttons(0, BUTTON_RETURN, false);
                break;
            case SDLK_p:
                update_buttons(0, BUTTON_PAUSE, false);
                break;
            case SDLK_SPACE:
                update_buttons(0, BUTTON_SPACE, false);
                break;
            case SDLK_PAGEUP:
                update_buttons(0, BUTTON_PAGE_UP, false);
                break;
            case SDLK_PAGEDOWN:
                update_buttons(0, BUTTON_PAGE_DOWN, false);
                break;
            case INPUT_ESCAPE:
                update_buttons(0, BUTTON_ESCAPE, false);
                break;
            default:
                break;
            }
            {
                if (event.key.keysym.scancode > 0 && event.key.keysym.scancode < NUM_SCANCODES)
                    m_scancodes[event.key.keysym.scancode] = false;
                m_mouse_keymod = event.key.keysym.mod;
            }
            break;
        case SDL_QUIT:
            p8_quit();
            break;
        default:
            break;
        }
    }
#elif defined(FEMTO8_ESPBOX)
    m_buttons[0] = p8_espbox_buttons();
    m_memory[MEMORY_BUTTON_STATE] = m_buttons[0] & 0xff;
#elif defined(OS_FREERTOS)
    uint16_t mask = 0;

    if (gamepad & AXIS_L_LEFT)
        mask |= BUTTON_MASK_LEFT;
    if (gamepad & AXIS_L_RIGHT)
        mask |= BUTTON_MASK_RIGHT;
    if (gamepad & AXIS_L_UP)
        mask |= BUTTON_MASK_UP;
    if (gamepad & AXIS_L_DOWN)
        mask |= BUTTON_MASK_DOWN;
    if (gamepad & AXIS_L_TRIGGER)
        mask |= BUTTON_MASK_ACTION1;
    if (gamepad & AXIS_R_LEFT)
        mask |= BUTTON_MASK_LEFT;
    if (gamepad & AXIS_R_RIGHT)
        mask |= BUTTON_MASK_RIGHT;
    if (gamepad & AXIS_R_UP)
        mask |= BUTTON_MASK_UP;
    if (gamepad & AXIS_R_DOWN)
        mask |= BUTTON_MASK_DOWN;
    if (gamepad & AXIS_R_TRIGGER)
        mask |= BUTTON_MASK_ACTION2;
    if (gamepad & DPAD_UP)
        mask |= BUTTON_MASK_UP;
    if (gamepad & DPAD_RIGHT)
        mask |= BUTTON_MASK_RIGHT;
    if (gamepad & DPAD_DOWN)
        mask |= BUTTON_MASK_DOWN;
    if (gamepad & DPAD_LEFT)
        mask |= BUTTON_MASK_LEFT;
    if (gamepad & BUTTON_1)
        mask |= BUTTON_MASK_ACTION1;
    if (gamepad & BUTTON_2)
        mask |= BUTTON_MASK_ACTION2;

    m_buttons[0] = mask;
#endif

    uint8_t delay = m_memory[MEMORY_AUTO_REPEAT_DELAY];
    if (delay == 0)
        delay = DEFAULT_AUTO_REPEAT_DELAY;
    uint8_t interval = m_memory[MEMORY_AUTO_REPEAT_INTERVAL];
    if (interval == 0)
        interval = DEFAULT_AUTO_REPEAT_INTERVAL;
    for (unsigned p=0;p<PLAYER_COUNT;++p) {
        m_buttonsp[p] = 0;
        for (unsigned i=0;i<BUTTON_INTERNAL_COUNT;++i) {
            if (m_buttons[p] & (1 << i)) {
                if (m_button_down_time[p][i] == UINT_MAX) {
                    // ignore buttons pressed at startup
                } else if (!m_button_down_time[p][i]) {
                    m_button_down_time[p][i] = m_frames;
                    m_buttonsp[p] |= 1 << i;
                } else if (i < BUTTON_REPEAT_COUNT) {
                    if (delay != 255 && !(m_button_first_repeat[p] & (1 << i)) && m_frames - m_button_down_time[p][i] >= delay) {
                        m_button_down_time[p][i] = m_frames;
                        m_button_first_repeat[p] |= 1 << i;
                        m_buttonsp[p] |= 1 << i;
                    } else if ((m_button_first_repeat[p] & (1 << i)) && m_frames - m_button_down_time[p][i] >= interval) {
                        m_button_down_time[p][i] = m_frames;
                        m_buttonsp[p] |= 1 << i;
                    }
                }
            } else  {
#ifdef SDL
                // Catch a press-and-release within one frame via latch
                if ((m_buttons_latch[p] & (1 << i)) && !m_button_down_time[p][i]) {
                    m_buttonsp[p] |= 1 << i;
                }
#endif
                if (m_button_down_time[p][i]) {
                    m_button_down_time[p][i] = 0;
                    m_button_first_repeat[p] &= ~(1 << i);
                }
            }
        }
#ifdef SDL
        m_buttons_latch[p] = 0;
#endif
    }
}
