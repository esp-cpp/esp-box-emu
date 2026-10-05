/**
 * Copyright (C) 2025 Chris January
 */

#ifndef P8_PAUSE_MENU_H
#define P8_PAUSE_MENU_H

#include <stdbool.h>

#define MAX_CUSTOM_MENUITEMS 5

typedef struct {
    bool active;
    char label[64];
    int lua_callback_ref;   /* LUA_NOREF if no callback */
} p8_custom_menuitem_t;

extern p8_custom_menuitem_t m_custom_menuitems[MAX_CUSTOM_MENUITEMS];

extern void p8_show_pause_menu(void);

/**
 * Register or update a custom pause menu item.
 * @param index 1-based item index (1..MAX_CUSTOM_MENUITEMS)
 * @param label Display text (copied internally)
 * @param lua_callback_ref Lua registry reference for callback, or LUA_NOREF
 */
void p8_menuitem_set(int index, const char *label, int lua_callback_ref);

/**
 * Update only the label of an existing custom pause menu item.
 * The callback is preserved; the item is made active.
 * @param index 1-based item index (1..MAX_CUSTOM_MENUITEMS)
 * @param label New display text (copied internally)
 */
void p8_menuitem_set_label(int index, const char *label);

/**
 * Remove a custom pause menu item.
 * @param index 1-based item index
 */
void p8_menuitem_clear(int index);

/**
 * Release all Lua callback refs and clear all custom menu items.
 * Must be called with a valid lua_State before closing it.
 */
void p8_menuitem_reset_all(void);

#endif
