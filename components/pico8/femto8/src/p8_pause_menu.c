/**
 * Copyright (C) 2025 Chris January
 */

#include <stdbool.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "strtcpy.h"

#include "p8_pause_menu.h"
#include "p8_emu.h"
#include "p8_dialog.h"
#include "p8_input.h"
#include "p8_overlay_helper.h"
#include "p8_options.h"
#include "p8_audio.h"
#include "lua.h"
#include "lauxlib.h"

// Global Lua state (defined in p8_lua.c)
extern lua_State *L;

// Custom menu item storage
p8_custom_menuitem_t m_custom_menuitems[MAX_CUSTOM_MENUITEMS];

void p8_menuitem_set(int index, const char *label, int lua_callback_ref)
{
    if (index < 1 || index > MAX_CUSTOM_MENUITEMS)
        return;
    p8_custom_menuitem_t *item = &m_custom_menuitems[index - 1];
    // Release old callback ref if any
    if (item->lua_callback_ref != LUA_NOREF && L)
        luaL_unref(L, LUA_REGISTRYINDEX, item->lua_callback_ref);
    item->active = true;
    strtcpy(item->label, label ? label : "", sizeof(item->label));
    item->lua_callback_ref = lua_callback_ref;
}

void p8_menuitem_set_label(int index, const char *label)
{
    if (index < 1 || index > MAX_CUSTOM_MENUITEMS)
        return;
    p8_custom_menuitem_t *item = &m_custom_menuitems[index - 1];
    item->active = true;
    strtcpy(item->label, label ? label : "", sizeof(item->label));
}

void p8_menuitem_clear(int index)
{
    if (index < 1 || index > MAX_CUSTOM_MENUITEMS)
        return;
    p8_custom_menuitem_t *item = &m_custom_menuitems[index - 1];
    if (item->lua_callback_ref != LUA_NOREF && L)
        luaL_unref(L, LUA_REGISTRYINDEX, item->lua_callback_ref);
    item->active = false;
    item->label[0] = '\0';
    item->lua_callback_ref = LUA_NOREF;
}

void p8_menuitem_reset_all(void)
{
    for (int i = 0; i < MAX_CUSTOM_MENUITEMS; i++) {
        if (m_custom_menuitems[i].lua_callback_ref != LUA_NOREF && L)
            luaL_unref(L, LUA_REGISTRYINDEX, m_custom_menuitems[i].lua_callback_ref);
        m_custom_menuitems[i].active = false;
        m_custom_menuitems[i].label[0] = '\0';
        m_custom_menuitems[i].lua_callback_ref = LUA_NOREF;
    }
}

// Call a custom menuitem Lua callback.
// button: button bitmask (1=left, 2=right, 112=selected/O+X+Pause)
// Returns true if the callback returned a truthy value (keep menu open).
static bool call_menuitem_callback(int lua_callback_ref, int button)
{
    if (lua_callback_ref == LUA_NOREF || !L)
        return false;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_callback_ref);
    lua_pushinteger(L, button);
    bool stay_open = false;
    if (lua_pcall(L, 1, 1, 0) == 0) {
        stay_open = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    return stay_open;
}

// Action IDs for custom menu items: index 0..4 → 100..104
#define CUSTOM_MENUITEM_ACTION_BASE 100

void p8_show_pause_menu(void)
{
    if (m_dialog_showing)
        return;

    /* 0x5f30: suppress next pause menu activation */
    if (m_memory[MEMORY_SUPPRESS_PAUSE] == 1) {
        m_memory[MEMORY_SUPPRESS_PAUSE] = 0;
        return;
    }

    /* Pause audio during pause menu unless 0x5f2f == 2 */
    bool should_pause_audio = m_memory[MEMORY_AUDIO_PAUSE] != 2;
    if (should_pause_audio)
        audio_pause();

    // Build dynamic controls array:
    // "continue" + up to 5 custom items + "restart" + "show version" + "controls" + "quit"
    p8_dialog_control_t pause_controls[5 + MAX_CUSTOM_MENUITEMS];
    int num_controls = 0;

    pause_controls[num_controls++] = (p8_dialog_control_t)DIALOG_MENUITEM("continue", 0);

    for (int i = 0; i < MAX_CUSTOM_MENUITEMS; i++) {
        if (m_custom_menuitems[i].active) {
            pause_controls[num_controls++] = (p8_dialog_control_t)
                DIALOG_MENUITEM(m_custom_menuitems[i].label,
                                CUSTOM_MENUITEM_ACTION_BASE + i);
        }
    }

    pause_controls[num_controls++] = (p8_dialog_control_t)DIALOG_MENUITEM("restart", 1);
    pause_controls[num_controls++] = (p8_dialog_control_t)DIALOG_MENUITEM("show version", 2);
    pause_controls[num_controls++] = (p8_dialog_control_t)DIALOG_MENUITEM("controls", 3);
    pause_controls[num_controls++] = (p8_dialog_control_t)DIALOG_MENUITEM("quit", 4);

    p8_dialog_t pause_dialog;
    p8_dialog_init(&pause_dialog, NULL, pause_controls, num_controls, P8_WIDTH / 2);

    // Run dialog loop, allowing menuitem callbacks to keep the menu open
    p8_dialog_set_showing(&pause_dialog, true);

    int action_id = -1;
    while (!p8_is_quit_requested()) {
        p8_dialog_draw(&pause_dialog);
        p8_flip();
        p8_dialog_action_t result = p8_dialog_update(&pause_dialog);

        if (result.type == DIALOG_RESULT_NONE)
            continue;

        if (result.type == DIALOG_RESULT_CANCELLED) {
            // Escape pressed: treat as "continue"
            break;
        }

        if (result.type == DIALOG_RESULT_BUTTON) {
            action_id = result.action_id;

            if (action_id >= CUSTOM_MENUITEM_ACTION_BASE &&
                action_id < CUSTOM_MENUITEM_ACTION_BASE + MAX_CUSTOM_MENUITEMS) {
                // Custom menu item: translate button_mask to PICO-8 button bitmask
                int idx = action_id - CUSTOM_MENUITEM_ACTION_BASE;
                int button;
                if (result.button_mask & BUTTON_MASK_LEFT)
                    button = 1;
                else if (result.button_mask & BUTTON_MASK_RIGHT)
                    button = 2;
                else
                    button = 112; // Selected: O + X + Pause bits combined

                bool stay = call_menuitem_callback(
                    m_custom_menuitems[idx].lua_callback_ref, button);
                if (stay)
                    continue; // Keep menu open
                break;
            }
            break;
        }

        break;
    }

    p8_dialog_set_showing(&pause_dialog, false);
    p8_dialog_cleanup(&pause_dialog);
    p8_flip(); // Ensure dialog hides immediately even if next flip is some time away

    if (should_pause_audio)
        audio_resume();

    switch (action_id) {
        case 0: // Continue
            break;
        case 1: // Restart
            p8_restart();
            break;
        case 2: // Show version
            p8_show_version_dialog();
            break;
        case 3: // Controls
            p8_show_controls_dialog();
            break;
        case 4: // Quit
            p8_abort();
            break;
        default:
            break;
    }
}
