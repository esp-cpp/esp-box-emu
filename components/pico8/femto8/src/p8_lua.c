
/*
 * p8_lua.c
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#include <assert.h>
#include "p8_symbols.h"
#include "pico_font.h"
#include "p8_audio.h"
#include "p8_browse.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_lua.h"
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>    // GetProcessMemoryInfo
#elif defined(__GLIBC__)
#if defined(__APPLE__)
  #include <stdlib.h>
  #include <malloc/malloc.h>
#else
  #include <malloc.h> // mallinfo / mallinfo2
#endif
#else
#include <unistd.h>   // sysconf
#include <stdio.h>    // fopen/fscanf
#endif
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "p8_lua_helper.h"
#include "p8_print_helper.h"
#include "pico_font.h"
#include "p8_parser.h"
#include "p8_cstore.h"
#include "p8_pause_menu.h"
#include "lua_api.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "strtcpy.h"

lua_State *L = NULL;
static int m_status = 0;

const void *m_lua_init = NULL;
const void *m_lua_update = NULL;
const void *m_lua_update60 = NULL;
const void *m_lua_draw = NULL;

/* m_clipboard is a fixed-size buffer defined in p8_emu.c */

char m_str_buffer[256] = {0};

static int m_tline_precision = 13;

static int m_load_result = 0;  /* stat(107): 1=success, -1=not found, -2=fetch failed, -3=no bbs */

int lua_load_api();
int lua_shutdown_api();
void lua_print_error();
int lua_init_script(const char *file_name, const char *script);
int lua_call_function(const char *name, int ret);
int lua_update();
int lua_draw();
int lua_init();

void lua_register_functions(lua_State *L);

static unsigned addr_remap(unsigned address)
{
    if (address >= 0x0000 && address < 0x2000)
        address = (m_memory[MEMORY_SPRITE_PHYS] << 8) | (address & 0x1fff);
    else if (address >= 0x6000 && address < 0x8000)
        address = (m_memory[MEMORY_SCREEN_PHYS] << 8) | (address & 0x1fff);
    return address;
}

// ****************************************************************
// *** Graphics ***
// ****************************************************************

// camera([x,] [y])
int camera(lua_State *L)
{
    int prev_cx, prev_cy;
    camera_get(&prev_cx, &prev_cy);

    int cx = lua_gettop(L) >= 1 ? lua_tointeger(L, 1) : 0;
    int cy = lua_gettop(L) >= 2 ? lua_tointeger(L, 2) : 0;

    camera_set(cx, cy);

    lua_pushinteger(L, prev_cx);
    lua_pushinteger(L, prev_cy);
    return 2;
}

// circ(x, y, [r,] [col])
int circ(lua_State *L)
{
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    int r = lua_gettop(L) >= 3 ? lua_tointeger(L, 3) : 4;
    int col = lua_gettop(L) >= 4 ? lua_tointeger(L, 4) : pencolor_get();
    int fillp = lua_gettop(L) >= 4 ? (fix32_bits(lua_tonumber(L, 4)) & 0xffff) : 0;

    if (lua_gettop(L) >= 3 && (m_memory[MEMORY_MISCFLAGS] & 0x2)) {
        double r_real = fix32_to_double(lua_tonumber(L, 3));
        if (r_real - r >= 0.5)
            r++;
    }

    if (lua_gettop(L) >= 4)
        pencolor_set(col);

    draw_circ(x, y, r, col, fillp);

    return 0;
}

// circfill(x, y, [r,] [col])
int circfill(lua_State *L)
{
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    int r = lua_gettop(L) >= 3 ? lua_tointeger(L, 3) : 4;
    int col = lua_gettop(L) >= 4 ? lua_tointeger(L, 4) : pencolor_get();
    int fillp = lua_gettop(L) >= 4 ? (fix32_bits(lua_tonumber(L, 4)) & 0xffff) : 0;

    if (lua_gettop(L) >= 3 && (m_memory[MEMORY_MISCFLAGS] & 0x2)) {
        double r_real = fix32_to_double(lua_tonumber(L, 3));
        if (r_real - r >= 0.5)
            r++;
    }

    if (lua_gettop(L) >= 4)
        pencolor_set(col);

    draw_circfill(x, y, r, col, fillp);

    return 0;
}

// clip(x, y, w, h)
int clip(lua_State *L)
{
    int prev_x0, prev_y0, prev_x1, prev_y1;
    clip_get(&prev_x0, &prev_y0, &prev_x1, &prev_y1);

    if (lua_gettop(L) == 0)
        clip_set(0, 0, P8_WIDTH, P8_HEIGHT);
    else
    {
        int x0 = lua_tointeger(L, 1);
        int y0 = lua_tointeger(L, 2);
        int w = lua_tointeger(L, 3);
        int h = lua_tointeger(L, 4);
        bool clip_previous = lua_gettop(L) >= 5 ? lua_toboolean(L, 5) : false;

        int clamp_x0 = 0, clamp_y0 = 0, clamp_x1 = P8_WIDTH, clamp_y1 = P8_HEIGHT;
        if (clip_previous) { clamp_x0 = prev_x0; clamp_y0 = prev_y0; clamp_x1 = prev_x1; clamp_y1 = prev_y1; }

        int x1 = x0 + w;
        int y1 = y0 + h;
        x0 = MAX(x0, clamp_x0);
        y0 = MAX(y0, clamp_y0);
        x1 = MIN(x1, clamp_x1);
        y1 = MIN(y1, clamp_y1);

        clip_set(x0, y0, x1-x0, y1-y0);
    }

    lua_pushinteger(L, prev_x0);
    lua_pushinteger(L, prev_y0);
    lua_pushinteger(L, prev_x1 - prev_x0);
    lua_pushinteger(L, prev_y1 - prev_y0);
    return 4;
}

// cls([color])
int cls(lua_State *L)
{
    int color = lua_gettop(L) == 1 ? lua_tointeger(L, -1) : 0;

    clear_screen(color);

    return 0;
}

// color(col)
int color(lua_State *L)
{
    int prev = pencolor_get();
    int col = lua_gettop(L) == 1 ? lua_tointeger(L, 1) : 6;

    pencolor_set(col);

    lua_pushinteger(L, prev);
    return 1;
}

// cursor([x,] [y,] [col])
int cursor(lua_State *L)
{
    int prev_x, prev_y;
    cursor_get(&prev_x, &prev_y);

    int x = lua_gettop(L) >= 1 ? lua_tointeger(L, 1) : 0;
    int y = lua_gettop(L) >= 2 ? lua_tointeger(L, 2) : 0;
    int color = lua_gettop(L) >= 3 ? lua_tointeger(L, 3) : -1;

    cursor_set(x, y, (color == -1) ? -1 : color);
    left_margin_set(x);

    lua_pushinteger(L, prev_x);
    lua_pushinteger(L, prev_y);
    return 2;
}

// fget(n, [f])
int fget(lua_State *L)
{
    int n = lua_tointeger(L, 1);
    uint8_t flags = m_memory[MEMORY_SPRITEFLAGS + n];

    if (lua_gettop(L) == 2)
    {
        int f = lua_tointeger(L, 2);
        lua_pushboolean(L, (flags & (1 << f)) != 0);
    }
    else
    {
        lua_pushinteger(L, flags);
    }

    return 1;
}

// fillp([pat])
int fillp(lua_State *L)
{
    if (lua_gettop(L) == 0) {
        m_memory[MEMORY_FILLP] = 0;
        m_memory[MEMORY_FILLP + 1] = 0;
        m_memory[MEMORY_FILLP_ATTR] = 0;
    } else {
        uint32_t n = fix32_bits(lua_tonumber(L, 1));
        m_memory[MEMORY_FILLP] = (n >> 16) & 0xff;
        m_memory[MEMORY_FILLP + 1] = (n >> 24) & 0xff;
        m_memory[MEMORY_FILLP_ATTR] = ((n & 0x8000) ? 1 : 0) | ((n & 0x4000) ? 2 : 0) | ((n & 0x2000) ? 4: 0);

    }
    return 0;
}

// flip()
int flip(lua_State *L)
{
    p8_flip();

    return 0;
}

int holdframe(lua_State *L)
{
    (void)L;
    return 0;
}

int _update_buttons(lua_State *L)
{
    (void)L;
    p8_update_input();
    return 0;
}

// fset(n, [f,] v)
int fset(lua_State *L)
{
    int n = lua_tointeger(L, 1);
    assert(n >= 0 && n <= 255);

    if (lua_gettop(L) == 3)
    {
        int f = lua_tointeger(L, 2);
        bool v = lua_toboolean(L, 3);
        assert(f >= 0 && f <= 7);

        if (v)
            m_memory[MEMORY_SPRITEFLAGS + n] |= (1 << f);
        else
            m_memory[MEMORY_SPRITEFLAGS + n] &= ~(1 << f);
    }
    else
    {
        int f = lua_tointeger(L, 2);
        assert(f >= 0 && f <= 255);
        m_memory[MEMORY_SPRITEFLAGS + n] = f;
    }

    return 0;
}

// line([x0,] [y0,] x1, y1, [col])
int line(lua_State *L)
{
    int x0, y0, x1, y1, col, fillp, valid;

    if (lua_gettop(L) == 0) {
        m_memory[MEMORY_LINE_VALID] = 1;
    } else {
        if (lua_gettop(L) >= 4) {
            valid = 1;
            x0 = lua_tointeger(L, 1);
            y0 = lua_tointeger(L, 2);
            x1 = lua_tointeger(L, 3);
            y1 = lua_tointeger(L, 4);
            col = lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : pencolor_get();
            fillp = lua_gettop(L) >= 5 ? (fix32_bits(lua_tonumber(L, 5)) & 0xffff) : 0;
            if (lua_gettop(L) >= 5)
                pencolor_set(col);
        } else {
            valid = !m_memory[MEMORY_LINE_VALID];
            x0 = m_memory[MEMORY_LINE_X] | (m_memory[MEMORY_LINE_X + 1] << 8);
            y0 = m_memory[MEMORY_LINE_Y] | (m_memory[MEMORY_LINE_Y + 1] << 8);
            x1 = lua_tointeger(L, 1);
            y1 = lua_tointeger(L, 2);
            col = lua_gettop(L) >= 3 ? lua_tointeger(L, 3) : pencolor_get();
            fillp = lua_gettop(L) >= 3 ? (fix32_bits(lua_tonumber(L, 3)) & 0xffff) : 0;
            if (lua_gettop(L) >= 3)
                pencolor_set(col);
        }

        if (valid)
            draw_line(x0, y0, x1, y1, col, fillp);

        m_memory[MEMORY_LINE_X] = x1 & 0xff;
        m_memory[MEMORY_LINE_X + 1] = (x1 >> 8) & 0xff;
        m_memory[MEMORY_LINE_Y] = y1 & 0xff;
        m_memory[MEMORY_LINE_Y + 1] = (y1 >> 8) & 0xff;
        m_memory[MEMORY_LINE_VALID] = 0;
    }

    return 0;
}

// oval(x0, y0, x1, y1, [col])
int oval(lua_State *L)
{
    int x0 = lua_tointeger(L, 1);
    int y0 = lua_tointeger(L, 2);
    int x1 = lua_tointeger(L, 3);
    int y1 = lua_tointeger(L, 4);
    int col = lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : pencolor_get();
    int fillp = lua_gettop(L) >= 5 ? (fix32_bits(lua_tonumber(L, 5)) & 0xffff) : 0;

    if (lua_gettop(L) >= 5)
        pencolor_set(col);

    draw_oval(x0, y0, x1, y1, col, fillp);

    return 0;
}

// ovalfill(x0, y0, x1, y1, [col])
int ovalfill(lua_State *L)
{
    int x0 = lua_tointeger(L, 1);
    int y0 = lua_tointeger(L, 2);
    int x1 = lua_tointeger(L, 3);
    int y1 = lua_tointeger(L, 4);
    int col = lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : pencolor_get();
    int fillp = lua_gettop(L) >= 5 ? (fix32_bits(lua_tonumber(L, 5)) & 0xffff) : 0;

    if (lua_gettop(L) >= 5)
        pencolor_set(col);

    draw_ovalfill(x0, y0, x1, y1, col, fillp);

    return 0;
}

// pal(c0, c1, [p])
// pal(tbl, [p])
int pal(lua_State *L)
{
    if (lua_gettop(L) == 0)
    {
        reset_color();
    }
    else if (lua_istable(L, 1))
    {
        int p = lua_gettop(L) >= 2 ? lua_tointeger(L, 2) : PALTYPE_DRAW;
        lua_pushnil(L);
        while (lua_next(L, 1))
        {
            int c0 = lua_tointeger(L, -2);
            int c1 = lua_tointeger(L, -1);
            uint8_t new_val;
            if (p == PALTYPE_DRAW) {
                uint8_t old_val = color_get(p, c0);
                new_val = (c1 & 0xf) | (old_val & 0xf0);
            } else {
                new_val = c1 & 0xff;
            }
            color_set(p, c0, new_val);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    else if (lua_gettop(L) == 1)
    {
        int p = lua_tointeger(L, 1);
        if (p == PALTYPE_DRAW) {
            for (int i=0;i<16;++i)
                color_set(PALTYPE_DRAW, i, i == 0 ? i | 0x10 : i);
        } else if (p == PALTYPE_SCREEN) {
            for (int i=0;i<16;++i)
                color_set(PALTYPE_SCREEN, i, i);
        } else if (p == PALTYPE_SECONDARY) {
            color_set(PALTYPE_SECONDARY, 0, 0x00);
            color_set(PALTYPE_SECONDARY, 1, 0x01);
            color_set(PALTYPE_SECONDARY, 2, 0x12);
            color_set(PALTYPE_SECONDARY, 3, 0x13);
            color_set(PALTYPE_SECONDARY, 4, 0x24);
            color_set(PALTYPE_SECONDARY, 5, 0x15);
            color_set(PALTYPE_SECONDARY, 6, 0xd6);
            color_set(PALTYPE_SECONDARY, 7, 0x67);
            color_set(PALTYPE_SECONDARY, 8, 0x48);
            color_set(PALTYPE_SECONDARY, 9, 0x49);
            color_set(PALTYPE_SECONDARY, 10, 0x9a);
            color_set(PALTYPE_SECONDARY, 11, 0x3b);
            color_set(PALTYPE_SECONDARY, 12, 0xdc);
            color_set(PALTYPE_SECONDARY, 13, 0x5d);
            color_set(PALTYPE_SECONDARY, 14, 0x8e);
            color_set(PALTYPE_SECONDARY, 15, 0xef);
        }
    }
    else
    {
        int c0 = lua_tointeger(L, 1);
        int c1 = lua_tointeger(L, 2);
        int p = lua_gettop(L) == 3 ? lua_tointeger(L, 3) : PALTYPE_DRAW;

        uint8_t old_val = color_get(p, c0);
        uint8_t new_val;
        if (p == PALTYPE_DRAW) {
            new_val = (c1 & 0xf) | (old_val & 0xf0);
        } else {
            new_val = c1 & 0xff;
        }
        color_set(p, c0, new_val);

        // Return the old mapped color for this slot
        lua_pushinteger(L, old_val & 0xf);
        return 1;
    }

    return 0;
}

// palt([col,] [t])
int palt(lua_State *L)
{
    if (lua_gettop(L) == 0)
    {
        for (int col=0;col<16;++col) {
            uint8_t c = color_get(PALTYPE_DRAW, col);
            color_set(PALTYPE_DRAW, col, col == 0 ? (c | 0x10) : (c & 0xf));
        }
    }
    else if (lua_gettop(L) == 1)
    {
        int b = lua_tounsigned(L, 1);
        for (int col=0;col<16;++col) {
            int t = (b >> (15-col)) & 1;
            uint8_t c = color_get(PALTYPE_DRAW, col);

            color_set(PALTYPE_DRAW, col, t ? (c | 0x10) : (c & 0xF));
        }
    }
    else
    {
        int col = lua_tointeger(L, 1);
        int t = lua_toboolean(L, 2);
        uint8_t c = color_get(PALTYPE_DRAW, col);
        bool prev_transparent = (c & 0x10) != 0;

        color_set(PALTYPE_DRAW, col, t ? (c | 0x10) : (c & 0xF));

        lua_pushboolean(L, prev_transparent);
        return 1;
    }

    return 0;
}

// pget(x, y)
int pget(lua_State *L)
{
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);

    int x0, y0, x1, y1;
    clip_get(&x0, &y0, &x1, &y1);

    if (x >= x0 && y >= y0 && x < x1 && y < y1) {
        lua_pushinteger(L, gfx_get(x, y, MEMORY_SCREEN, MEMORY_SCREEN_SIZE));
    } else {
        int default_val = (m_memory[MEMORY_MISCFLAGS] & 0x10) ? m_memory[MEMORY_PGET_DEFAULT] : 0;
        lua_pushinteger(L, default_val);
    }

    return 1;
}

// print(str, [x,] [y,] [col])
int print(lua_State *L)
{
    size_t len = 0;
    const char *str = lua_gettop(L) >= 1 ? lua_tolstring(L, 1, &len) : NULL;
    if (!str) { str = ""; len = 0; }
    int right;

    if (lua_gettop(L) <= 2)
    {
        int x, y;
        cursor_get(&x, &y);
        int col = lua_gettop(L) == 2 ? lua_tointeger(L, 2) : pencolor_get();
        if (lua_gettop(L) == 2)
            pencolor_set(col);
        draw_text(str, len, x, y, col, left_margin_get(), false, &x, &y, &right);
        cursor_set(x, y, -1);
    }
    else
    {
        int x = lua_tointeger(L, 2);
        int y = lua_tointeger(L, 3);
        int col = lua_gettop(L) >= 4 ? lua_tointeger(L, 4) : pencolor_get();
        if (lua_gettop(L) >= 4)
            pencolor_set(col);
        left_margin_set(x);
        draw_text(str, len, x, y, col, x, true, &x, &y, &right);
        cursor_set(x, y, -1);
    }

    lua_pushinteger(L, right);
    return 1;
}

// pset(x, y, [c])
int pset(lua_State *L)
{
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    int c = lua_gettop(L) == 3 ? lua_tointeger(L, 3) : pencolor_get();
    int fillp = lua_gettop(L) >= 3 ? (fix32_bits(lua_tonumber(L, 3)) & 0xffff) : 0;
    if (lua_gettop(L) == 3)
        pencolor_set(c);
    pixel_set(x, y, c, fillp, DRAWTYPE_GRAPHIC);

    return 0;
}

// rect(x0, y0, x1, y1, [col])
int rect(lua_State *L)
{
    int x0 = lua_tointeger(L, 1);
    int y0 = lua_tointeger(L, 2);
    int x1 = lua_tointeger(L, 3);
    int y1 = lua_tointeger(L, 4);
    int col = lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : pencolor_get();
    int fillp = lua_gettop(L) >= 5 ? (fix32_bits(lua_tonumber(L, 5)) & 0xffff) : 0;

    int left = MIN(x0, x1);
    int top = MIN(y0, y1);
    int right = MAX(x0, x1);
    int bottom = MAX(y0, y1);

    if (lua_gettop(L) >= 5)
        pencolor_set(col);

    draw_rect(left, top, right, bottom, col, fillp);

    return 0;
}

// rectfill(x0, y0, x1, y1, [col])
int rectfill(lua_State *L)
{
    int x0 = lua_tointeger(L, 1);
    int y0 = lua_tointeger(L, 2);
    int x1 = lua_tointeger(L, 3);
    int y1 = lua_tointeger(L, 4);
    int col = lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : pencolor_get();
    int fillp = lua_gettop(L) >= 5 ? (fix32_bits(lua_tonumber(L, 5)) & 0xffff) : 0;

    int left = MIN(x0, x1);
    int top = MIN(y0, y1);
    int right = MAX(x0, x1);
    int bottom = MAX(y0, y1);

    if (lua_gettop(L) >= 5)
        pencolor_set(col);

    draw_rectfill(left, top, right, bottom, col, fillp);

    return 0;
}

// rrect(x, y, w, h, r, [col])
int rrect(lua_State *L)
{
    int left = lua_tointeger(L, 1);
    int top = lua_tointeger(L, 2);
    int w = lua_tointeger(L, 3);
    int h = lua_tointeger(L, 4);
    int r = lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : 0;
    int col = lua_gettop(L) >= 6 ? lua_tointeger(L, 6) : pencolor_get();
    int fillp = lua_gettop(L) >= 6 ? (fix32_bits(lua_tonumber(L, 6)) & 0xffff) : 0;

    if (lua_gettop(L) >= 6)
        pencolor_set(col);

    int right = left + w-1;
    int bottom = top + h-1;
    r = MAX(0, MIN(r, MIN(w, h) / 2));

    if (is_bbox_offscreen(left, top, right, bottom)) return 0;

    draw_hline(left+r, top, right-r, col, fillp);
    draw_hline(left+r, bottom, right-r, col, fillp);
    draw_vline(left, top+r, bottom-r, col, fillp);
    draw_vline(right, top+r, bottom-r, col, fillp);
    draw_circ_mask(left+r, top+r, r, col, fillp, 1 << 3 | 1 << 7);
    draw_circ_mask(right-r, top+r, r, col, fillp, 1 << 2 | 1 << 6);
    draw_circ_mask(left+r, bottom-r, r, col, fillp, 1 << 1 | 1 << 5);
    draw_circ_mask(right-r, bottom-r, r, col, fillp, 1 << 0 | 1 << 4);

    return 0;
}

// rrectfill(x, y, w, h, r, [col])
int rrectfill(lua_State *L)
{
    int left = lua_tointeger(L, 1);
    int top = lua_tointeger(L, 2);
    int w = lua_tointeger(L, 3);
    int h = lua_tointeger(L, 4);
    int r = lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : 0;
    int col = lua_gettop(L) >= 6 ? lua_tointeger(L, 6) : pencolor_get();
    int fillp = lua_gettop(L) >= 6 ? (fix32_bits(lua_tonumber(L, 6)) & 0xffff) : 0;
    bool invert = fillp_invert_enabled(col);

    if (lua_gettop(L) >= 6)
        pencolor_set(col);

    int right = left + w;
    int bottom = top + h;
    r = MAX(0, MIN(r, MIN(w, h) / 2));

    if (!invert && is_bbox_offscreen(left, top, right, bottom)) return 0;

    if (invert) {
        int cx, cy;
        camera_get(&cx, &cy);
        int clip_x0, clip_y0, clip_x1, clip_y1;
        clip_get(&clip_x0, &clip_y0, &clip_x1, &clip_y1);
        for (int y = clip_y0; y < clip_y1; y++) {
            for (int x = clip_x0; x < clip_x1; x++) {
                int wx = x + cx;
                int wy = y + cy;
                if (!point_in_round_rect(wx, wy, left, top, right, bottom, r))
                    pixel_set(wx, wy, col, fillp, DRAWTYPE_GRAPHIC);
            }
        }
        return 0;
    }

    draw_rectfill(left, top+r, right, bottom-r, col, fillp);
    draw_rectfill(left+r, top, right-r, top+r, col, fillp);
    draw_rectfill(left+r, bottom-r, right-r, bottom, col, fillp);
    draw_circfill_mask(left+r, top+r, r, col, fillp, 1 << 3 | 1 << 7);
    draw_circfill_mask(right-r, top+r, r, col, fillp, 1 << 2 | 1 << 6);
    draw_circfill_mask(left+r, bottom-r, r, col, fillp, 1 << 1 | 1 << 5);
    draw_circfill_mask(right-r, bottom-r, r, col, fillp, 1 << 0 | 1 << 4);

    return 0;
}

// sget(x, y)
int sget(lua_State *L)
{
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);

    if (x >= 0 && y >= 0 && x < P8_WIDTH && y < P8_HEIGHT) {
        lua_pushinteger(L, gfx_get(x, y, MEMORY_SPRITES, MEMORY_SPRITES_SIZE));
    } else {
        int default_val = (m_memory[MEMORY_MISCFLAGS] & 0x10) ? m_memory[MEMORY_SGET_DEFAULT] : 0;
        lua_pushinteger(L, default_val);
    }

    return 1;
}

// spr(n, x, y, [w,] [h,] [flip_x,] [flip_y])
int spr(lua_State *L)
{
    int n = lua_tointeger(L, 1);
    int x = lua_gettop(L) >= 2 ? lua_tointeger(L, 2) : 0;
    int y = lua_gettop(L) >= 3 ? lua_tointeger(L, 3) : 0;
    bool flip_x = false, flip_y = false;

    if (lua_gettop(L) > 3)
    {
        assert(lua_gettop(L) >= 5);

        lua_Number w_raw = lua_tonumber(L, 4);
        lua_Number h_raw = lua_tonumber(L, 5);

        if (lua_gettop(L) >= 6)
            flip_x = lua_toboolean(L, 6);

        if (lua_gettop(L) >= 7)
            flip_y = lua_toboolean(L, 7);

        // fix32 stores integers with no fractional bits (lower 16 bits == 0)
        if ((w_raw & 0xFFFF) == 0 && (h_raw & 0xFFFF) == 0)
        {
            // Integer w/h: draw a w x h grid of sprites (standard path)
            int w = fix32_to_int(w_raw);
            int h = fix32_to_int(h_raw);
            draw_sprites(n, x, y, w, h, flip_x, flip_y);
        }
        else
        {
            // Fractional w/h: compute pixel dimensions and draw at 1:1 scale
            int sw = (int)(fix32_to_double(w_raw) * SPRITE_WIDTH + 0.5);
            int sh = (int)(fix32_to_double(h_raw) * SPRITE_HEIGHT + 0.5);
            int ssx = (n & 0xF) * SPRITE_WIDTH;
            int ssy = (n >> 4) * SPRITE_HEIGHT;
            draw_scaled_sprite(ssx, ssy, sw, sh, x, y, 1.0f, 1.0f, flip_x, flip_y);
        }
    }
    else
    {
        draw_sprites(n, x, y, 1, 1, flip_x, flip_y);
    }

    return 0;
}

// sset(x, y, [c])
int sset(lua_State *L)
{
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    int c = lua_gettop(L) >= 3 ? lua_tointeger(L, 3) : pencolor_get();

    if (lua_gettop(L) >= 3)
        pencolor_set(c);
    if (x >= 0 && y >= 0 && x < P8_WIDTH && y < P8_HEIGHT)
        gfx_set(x, y, MEMORY_SPRITES, MEMORY_SPRITES_SIZE, c);

    return 0;
}

// sspr(sx, sy, sw, sh, dx, dy, [dw,] [dh,] [flip_x,] [flip_y])
int sspr(lua_State *L)
{
    int sx = lua_tointeger(L, 1);
    int sy = lua_tointeger(L, 2);
    int sw = lua_tointeger(L, 3);
    int sh = lua_tointeger(L, 4);
    int dx = lua_tointeger(L, 5);
    int dy = lua_tointeger(L, 6);
    int dw = lua_to_or_default(L, integer, 7, sw);
    int dh = lua_to_or_default(L, integer, 8, sh);
    bool flip_x = lua_to_or_default(L, boolean, 9, false);
    bool flip_y = lua_to_or_default(L, boolean, 10, false);
    float scale_x = (float)dw / sw;
    float scale_y = (float)dh / sh;

    draw_scaled_sprite(sx, sy, sw, sh, dx, dy, scale_x, scale_y, flip_x, flip_y);

    return 0;
}

// tline( x0, y0, x1, y1, mx, my, [mdx,] [mdy])
// tline( precision )
int tline(lua_State *L)
{
    if (lua_gettop(L) == 1) {
        m_tline_precision = lua_tointeger(L, 1);
        return 0;
    }

    int x0 = lua_tointeger(L, 1);
    int y0 = lua_tointeger(L, 2);
    int x1 = lua_tointeger(L, 3);
    int y1 = lua_tointeger(L, 4);
    lua_Number mx = lua_tonumber(L, 5);
    lua_Number my = lua_tonumber(L, 6);
    lua_Number mdx = lua_to_or_default(L, number, 7, fix32_div(fix32_from_int(1), fix32_from_int(8)));
    lua_Number mdy = lua_to_or_default(L, number, 8, fix32_from_int(0));
    int layer = lua_to_or_default(L, integer, 9, 0);

    int cx, cy;
    camera_get(&cx, &cy);
    int clip_x0, clip_y0, clip_x1, clip_y1;
    clip_get(&clip_x0, &clip_y0, &clip_x1, &clip_y1);

    uint8_t map_start = m_memory[MEMORY_MAP_START];
    bool map_invalid = (map_start >= 0x10 && map_start < 0x20) ||
                       (map_start >= 0x30 && map_start < 0x3f);
    uint8_t map_start_upper = map_start;
    uint8_t map_start_lower = map_start;
    if (!map_invalid) {
        if (map_start < 0x10 || (map_start >= 0x40 && map_start < 0x80))
            map_start_upper = 0x20;
        else
            map_start_upper = map_start;
        map_start_lower = (map_start_upper < 0x80) ? 0x10 : map_start_upper;
    }
    int map_width = m_memory[MEMORY_MAP_WIDTH];
    if (map_width == 0) map_width = 256;

    int sprite_base = m_memory[MEMORY_SPRITE_PHYS] << 8;
    int screen_base = m_memory[MEMORY_SCREEN_PHYS] << 8;

    int offset_x = m_memory[MEMORY_TLINE_OFFSET_X] * 8;
    int offset_y = m_memory[MEMORY_TLINE_OFFSET_Y] * 8;
    int precision = m_tline_precision;

    uint32_t mask_x_bits = ((uint32_t)m_memory[MEMORY_TLINE_MASK_X] << (precision + 3)) - 1;
    uint32_t mask_y_bits = ((uint32_t)m_memory[MEMORY_TLINE_MASK_Y] << (precision + 3)) - 1;

    uint32_t mx_bits = fix32_bits(mx);
    uint32_t my_bits = fix32_bits(my);
    uint32_t mdx_bits = fix32_bits(mdx);
    uint32_t mdy_bits = fix32_bits(mdy);

    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    uint8_t *draw_pal = &m_memory[MEMORY_PALETTES + PALTYPE_DRAW * 16];
    uint8_t *sprite_flags_base = &m_memory[MEMORY_SPRITEFLAGS];
    bool sprite_0_opaque = (m_memory[MEMORY_MISCFLAGS] & 0x8) != 0;
    uint8_t rw_mask = m_memory[MEMORY_RW_MASK];

    while (true)
    {
        uint32_t tmx = mx_bits & mask_x_bits;
        uint32_t tmy = my_bits & mask_y_bits;

        int tx_raw = ((int32_t)tmx) >> precision;
        int ty_raw = ((int32_t)tmy) >> precision;
        int tx = tx_raw + offset_x;
        int ty = ty_raw + offset_y;

        int celx = tx >> 3;
        int cely = ty >> 3;
        int index = 0;
        // Bounds check uses pre-offset coordinates (PICO-8 behaviour: offset doesn't
        // make a negative texture coordinate valid for map lookup).
        if (!map_invalid && (tx_raw >> 3) >= 0 && (ty_raw >> 3) >= 0) {
            uint8_t ms = (cely >= 32 && map_start_upper < 0x80) ? map_start_lower : map_start_upper;
            int adj_cely = (cely >= 32 && map_start_upper < 0x80) ? cely - 32 : cely;
            int address = (ms << 8) + celx + adj_cely * map_width;
            if (address >= 0x1000 && address < 0x10000 &&
                !(address >= 0x3000 && address < 0x8000))
                index = m_memory[address];
        }
        if ((index != 0 || sprite_0_opaque) && (layer == 0 || ((layer & sprite_flags_base[index]) == layer))) {
            int spx = (index & 0xF) * 8 + (tx & 7);
            int spy = (index >> 4) * 8 + (ty & 7);

            int spr_offset = sprite_base + (spx >> 1) + spy * 64;
            uint8_t col = IS_EVEN(spx) ? m_memory[spr_offset] & 0xF : m_memory[spr_offset] >> 4;

            uint8_t mapped = draw_pal[col & 0xf];
            int px = x0 - cx;
            int py = y0 - cy;
            if ((mapped & 0xf0) == 0) {
                if (px >= clip_x0 && px < clip_x1 && py >= clip_y0 && py < clip_y1) {
                    int scr_offset = screen_base + (px >> 1) + py * 64;
                    if (rw_mask != 0xff) {
                        uint8_t write_mask = rw_mask & 0xf;
                        uint8_t read_mask = (rw_mask >> 4) & 0xf;
                        uint8_t dst = IS_EVEN(px) ? m_memory[scr_offset] & 0xf : m_memory[scr_offset] >> 4;
                        uint8_t src = mapped & 0xf;
                        uint8_t result = (dst & ~write_mask) | (src & write_mask & read_mask);
                        m_memory[scr_offset] = IS_EVEN(px)
                            ? (m_memory[scr_offset] & 0xF0) | result
                            : (result << 4) | (m_memory[scr_offset] & 0xF);
                    } else {
                        m_memory[scr_offset] = IS_EVEN(px)
                            ? (m_memory[scr_offset] & 0xF0) | (mapped & 0xF)
                            : (mapped << 4) | (m_memory[scr_offset] & 0xF);
                    }
                }
            }
        }

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }

        mx_bits += mdx_bits;
        my_bits += mdy_bits;
    }

    return 0;
}

// ****************************************************************
// *** Tables ***
// ****************************************************************

// add(tbl, v)
// all(tbl)
// count(tbl)
// del(tbl, v)
// foreach(tbl, func)
// pairs(tbl)

// ****************************************************************
// *** Input ***
// ****************************************************************

// btn([i,] [p])
int btn(lua_State *L)
{
    // button
    if (lua_gettop(L) >= 1)
    {
        int i = lua_tointeger(L, 1);
        int p = lua_gettop(L) >= 2 ? lua_tointeger(L, 2) : 0;

        if (i < BUTTON_COUNT && p < PLAYER_COUNT)
            lua_pushboolean(L, is_button_set(p, i, false));
        else
            lua_pushboolean(L, false);
    }
    // mask
    else
    {
        lua_pushinteger(L, (m_buttons[0] & 0xff) | ((m_buttons[1] & 0xff) << 8));
    }

    return 1;
}

// btnp([i,] [p])
int btnp(lua_State *L)
{
    // button
    if (lua_gettop(L) >= 1)
    {
        int i = lua_tointeger(L, 1);
        int p = lua_gettop(L) >= 2 ? lua_tointeger(L, 2) : 0;

        if (i < BUTTON_COUNT && p < PLAYER_COUNT)
            lua_pushboolean(L, is_button_set(p, i, true));
        else
            lua_pushboolean(L, false);
    }
    // mask
    else
    {
        lua_pushinteger(L, (m_buttonsp[0] & 0xff) | ((m_buttons[1] & 0xff) << 8));
    }

    return 1;
}

// ****************************************************************
// *** Sound ***
// ****************************************************************

// music(n, [fadems,] [channelmask])
int music(lua_State *L)
{
#ifdef ENABLE_AUDIO
    int prev = audio_stat(54);  // stat(54) = current music pattern, -1 if none

    int n = lua_tointeger(L, 1);
    int fadems = lua_to_or_default(L, integer, 2, 1);
    int channelmask = lua_to_or_default(L, integer, 3, 0);

    audio_music(n, fadems, channelmask);

    lua_pushinteger(L, prev);
    return 1;
#else
    lua_pushinteger(L, -1);
    return 1;
#endif
}

// sfx(n, [channel,] [offset,] [length])
int sfx(lua_State *L)
{
#ifdef ENABLE_AUDIO
    int n = lua_tointeger(L, 1);
    int channel = lua_to_or_default(L, integer, 2, -1);
    int offset = lua_to_or_default(L, integer, 3, 0);
    int length = lua_to_or_default(L, integer, 4, 32 - offset);

    audio_sound(n, channel, offset, length);
#endif

    return 0;
}

// ****************************************************************
// *** Map ***
// ****************************************************************

static void map_get_visible_cells(int sx, int sy, int celw, int celh,
                                   int *start_x, int *start_y, int *end_x, int *end_y)
{
    int cam_x, cam_y;
    camera_get(&cam_x, &cam_y);

    int clip_x0, clip_y0, clip_x1, clip_y1;
    clip_get(&clip_x0, &clip_y0, &clip_x1, &clip_y1);

    int screen_x0 = cam_x + clip_x0;
    int screen_y0 = cam_y + clip_y0;
    int screen_x1 = cam_x + clip_x1;
    int screen_y1 = cam_y + clip_y1;

    int visible_x0 = (screen_x0 - sx) / SPRITE_WIDTH;
    int visible_y0 = (screen_y0 - sy) / SPRITE_HEIGHT;
    int visible_x1 = (screen_x1 - sx + SPRITE_WIDTH - 1) / SPRITE_WIDTH;
    int visible_y1 = (screen_y1 - sy + SPRITE_HEIGHT - 1) / SPRITE_HEIGHT;

    *start_x = MAX(0, visible_x0);
    *start_y = MAX(0, visible_y0);
    *end_x = MIN(celw, visible_x1);
    *end_y = MIN(celh, visible_y1);
}

// map(celx, cely, sx, sy, celw, celh, [layer])
int map(lua_State *L)
{
    int default_celw = m_memory[MEMORY_MAP_WIDTH];
    if (default_celw == 0)
        default_celw = 128;
    int map_start = m_memory[MEMORY_MAP_START];
    int max_map_cells = (map_start >= 0x80) ? (0x10000 - (map_start << 8)) : 0x2000;
    int default_celh = (default_celw > 0) ? (max_map_cells / default_celw) : 64;

    int celx = lua_gettop(L) >= 1 ? lua_tointeger(L, 1) : 0;
    int cely = lua_gettop(L) >= 2 ? lua_tointeger(L, 2) : 0;
    int sx = lua_gettop(L) >= 3 ? lua_tointeger(L, 3) : 0;
    int sy = lua_gettop(L) >= 4 ? lua_tointeger(L, 4) : 0;
    int celw = lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : default_celw;
    int celh = lua_gettop(L) >= 6 ? lua_tointeger(L, 6) : default_celh;
    int layer = lua_gettop(L) >= 7 ? lua_tointeger(L, 7) : 0;

    int start_x, start_y, end_x, end_y;
    map_get_visible_cells(sx, sy, celw, celh, &start_x, &start_y, &end_x, &end_y);

    for (int y = start_y; y < end_y; y++)
    {
        for (int x = start_x; x < end_x; x++)
        {
            uint8_t index = map_get(celx + x, cely + y);
            uint8_t sprite_flags = m_memory[MEMORY_SPRITEFLAGS + index];
            bool sprite_0_opaque = (m_memory[MEMORY_MISCFLAGS] & 0x8) != 0;
            bool should_draw = (index != 0 || sprite_0_opaque) && (layer == 0 || ((layer & sprite_flags) == layer));

            if (should_draw)
            {
                int left = sx + x * SPRITE_WIDTH;
                int top = sy + y * SPRITE_HEIGHT;
                draw_sprite(index, left, top, false, false);
            }
        }
    }

    return 0;
}

// mget(celx, cely)
int mget(lua_State *L)
{
    int celx = lua_tointeger(L, 1);
    int cely = lua_tointeger(L, 2);

    int address = map_cell_addr(celx, cely);
    if (address == 0) {
        int default_val = (m_memory[MEMORY_MISCFLAGS] & 0x10) ? m_memory[MEMORY_MGET_DEFAULT] : 0;
        lua_pushinteger(L, default_val);
    } else {
        lua_pushinteger(L, m_memory[address]);
    }

    return 1;
}

// mset(celx, cely, snum)
int mset(lua_State *L)
{
    int celx = lua_tointeger(L, 1);
    int cely = lua_tointeger(L, 2);
    int snum = lua_tointeger(L, 3);

    map_set(celx, cely, snum);

    return 0;
}

// ****************************************************************
// *** Memory ***
// ****************************************************************

// cstore(destaddr, sourceaddr, len, [filename])
int cstore(lua_State *L)
{
    int nargs = lua_gettop(L);
    unsigned destaddr  = nargs >= 1 ? lua_tounsigned(L, 1) : 0;
    unsigned srcaddr   = nargs >= 2 ? lua_tounsigned(L, 2) : 0;
    unsigned len       = nargs >= 3 ? lua_tounsigned(L, 3) : CART_MEMORY_SIZE;
    const char *file_name = nargs >= 4 ? lua_tostring(L, 4) : NULL;

    if (destaddr + len > CART_MEMORY_SIZE || srcaddr + len > (1 << 16)) {
        lua_pushinteger(L, 0);
        return 1;
    }

    char resolved_path[PATH_MAX];
    if (file_name != NULL) {
        // Append .p8 if no extension given
        char full_filename[PATH_MAX];
        if (strstr(file_name, ".p8") == NULL && strstr(file_name, ".P8") == NULL) {
            snprintf(full_filename, sizeof(full_filename), "%s.p8", file_name);
            file_name = full_filename;
        }
        if (p8_resolve_relative_path(resolved_path, file_name, sizeof(resolved_path), true) < 0) {
            lua_pushinteger(L, 0);
            return 1;
        }
    } else {
        if (!m_current_cart_file_name[0]) {
            lua_pushinteger(L, 0);
            return 1;
        }
        strtcpy(resolved_path, m_current_cart_file_name, sizeof(resolved_path));
    }

    // Refuse to write .p8.png (not supported)
    size_t rlen = strlen(resolved_path);
    if (rlen >= 7 && strcmp(resolved_path + rlen - 7, ".p8.png") == 0) {
        fprintf(stderr, "cstore: .p8.png output is not supported\n");
        lua_pushinteger(L, 0);
        return 1;
    }

    p8_show_io_icon(true);

    int ret = MKDIR(DEFAULT_CARTS_PATH);
    if (ret == -1 && errno != EEXIST) {
        fprintf(stderr, "cstore: failed to create carts directory: %s\n", strerror(errno));
        p8_show_io_icon(false);
        lua_pushinteger(L, 0);
        return 1;
    }

    memset(m_temp_cart_memory, 0, CART_MEMORY_SIZE);
    m_temp_lua_script[0] = '\0';
    if (access(resolved_path, F_OK) == -1) {
        if (errno != ENOENT) {
            fprintf(stderr, "cstore: failed to access existing cart: %s\n", strerror(errno));
            p8_show_io_icon(false);
            lua_pushinteger(L, 0);
            return 1;
        }
    } else if (parse_cart_file(resolved_path, m_temp_cart_memory, m_file_buffer, m_decompression_buffer, m_temp_lua_script, NULL) != 0) {
        fprintf(stderr, "cstore: failed to read existing cart\n");
        p8_show_io_icon(false);
        lua_pushinteger(L, 0);
        return 1;
    }

    // Patch the requested region from runtime memory
    memcpy(m_temp_cart_memory + destaddr, m_memory + srcaddr, len);

    // Also update m_cart_memory when writing to the current cart
    if (nargs < 4 || lua_isnil(L, 4))
        memcpy(m_cart_memory + destaddr, m_memory + srcaddr, len);

    write_cart_p8(resolved_path, m_temp_lua_script, m_temp_cart_memory);

    p8_show_io_icon(false);
    lua_pushinteger(L, len);
    return 1;
}

// memcpy(destaddr, sourceaddr, len)
int _memcpy(lua_State *L)
{
    unsigned destaddr = lua_tounsigned(L, 1);
    unsigned orig_destaddr = destaddr;
    unsigned sourceaddr = lua_tounsigned(L, 2);
    unsigned len = lua_tounsigned(L, 3);

    if (len < 1 || destaddr == sourceaddr)
    {
        lua_pushinteger(L, orig_destaddr);
        return 1;
    }

    if (sourceaddr < 0 || (sourceaddr + len) > (1 << 16))
    {
        lua_pushinteger(L, orig_destaddr);
        return 1;
    }

    if (destaddr < 0 || (destaddr + len) > (1 << 16))
    {
        lua_pushinteger(L, orig_destaddr);
        return 1;
    }

    while (len > 0) {
        unsigned chunk = MIN(MIN(len, 0x2000 - (sourceaddr & 0x1fff)), 0x2000 - (destaddr & 0x1fff));
        unsigned destaddr1 = addr_remap(destaddr);
        unsigned sourceaddr1 = addr_remap(sourceaddr);
        memmove(m_memory + destaddr1, m_memory + sourceaddr1, chunk);
        destaddr += chunk;
        sourceaddr += chunk;
        len -= chunk;
    }

    lua_pushinteger(L, orig_destaddr);
    return 1;
}

// memset(destaddr, val, len)
int _memset(lua_State *L)
{
    unsigned destaddr = lua_tounsigned(L, 1);
    int val = lua_tointeger(L, 2);
    unsigned len = lua_tounsigned(L, 3);

    while (len > 0) {
        unsigned chunk = MIN(len, 0x2000 - (destaddr & 0x1fff));
        unsigned destaddr1 = addr_remap(destaddr);
        memset(m_memory + destaddr1, val, chunk);
        destaddr += chunk;
        len -= chunk;
    }

    return 0;
}

// peek(addr, [n])
int peek(lua_State *L)
{
    unsigned addr = lua_tounsigned(L, 1);
    unsigned n = lua_gettop(L) >= 2 ? lua_tounsigned(L, 2) : 1;

    addr = addr_remap(addr);

    luaL_checkstack(L, n, "too many values");

    for (unsigned i=0;i<n;++i)
        lua_pushinteger(L, m_memory[addr+i]);

    return n;
}

// peek2(addr, [n])
int peek2(lua_State *L)
{
    unsigned addr = lua_tounsigned(L, 1);
    unsigned n = lua_gettop(L) >= 2 ? lua_tounsigned(L, 2) : 1;

    addr = addr_remap(addr);

    luaL_checkstack(L, n, "too many values");

    for (unsigned i=0;i<n;++i)
        lua_pushinteger(L, (m_memory[addr + i*2 + 1] << 8) | (m_memory[addr + i*2]));

    return n;
}

// peek4(addr, [n])
int peek4(lua_State *L)
{
    unsigned addr = lua_tounsigned(L, 1);
    unsigned n = lua_gettop(L) >= 2 ? lua_tounsigned(L, 2) : 1;

    addr = addr_remap(addr);

    luaL_checkstack(L, n, "too many values");

    for (unsigned i=0;i<n;++i)
        lua_pushnumber(L, fix32_from_bits((m_memory[addr + i*4 + 3] << 24) | (m_memory[addr + i*4 + 2] << 16) | (m_memory[addr + i*4 + 1] << 8) | m_memory[addr + i*4]));

    return n;
}

// poke(addr, val1, ...)
int poke(lua_State *L)
{
    unsigned addr = lua_tounsigned(L, 1);

    addr = addr_remap(addr);

    for (int i=2;i<=lua_gettop(L);++i) {
        unsigned val = lua_tounsigned(L, i);

        m_memory[addr + i-2] = val;
    }

    if (addr >= MEMORY_CARTDATA && addr + 1 <= MEMORY_CARTDATA + MEMORY_CARTDATA_SIZE)
        p8_delayed_flush_cartdata();

    lua_pushinteger(L, 0);
    return 1;
}

// poke2(addr, val1, ...)
int poke2(lua_State *L)
{
    unsigned addr = lua_tounsigned(L, 1);

    addr = addr_remap(addr);

    for (int i=2;i<=lua_gettop(L);++i) {
        unsigned val = lua_tounsigned(L, i);

        m_memory[addr + (i-2)*2] = val;
        m_memory[addr + (i-2)*2 + 1] = val >> 8;
    }

    if (addr >= MEMORY_CARTDATA && addr + 2 <= MEMORY_CARTDATA + MEMORY_CARTDATA_SIZE)
        p8_delayed_flush_cartdata();

    lua_pushinteger(L, 0);
    return 1;
}

// poke4(addr, val1, ...)
int poke4(lua_State *L)
{
    unsigned addr = lua_tounsigned(L, 1);

    addr = addr_remap(addr);

    for (int i=2;i<=lua_gettop(L);++i) {
        uint32_t val = lua_tonumber(L, i);

        m_memory[addr + (i-2)*4] = val;
        m_memory[addr + (i-2)*4 + 1] = val >> 8;
        m_memory[addr + (i-2)*4 + 2] = val >> 16;
        m_memory[addr + (i-2)*4 + 3] = val >> 24;
    }

    if (addr >= MEMORY_CARTDATA && addr + 4 <= MEMORY_CARTDATA + MEMORY_CARTDATA_SIZE)
        p8_delayed_flush_cartdata();

    lua_pushinteger(L, 0);
    return 1;
}

// reload(destaddr, sourceaddr, len, [filename])
int reload(lua_State *L)
{
    int nargs = lua_gettop(L);
    unsigned destaddr = nargs >= 1 ? lua_tounsigned(L, 1) : 0;
    unsigned srcaddr  = nargs >= 2 ? lua_tounsigned(L, 2) : 0;
    unsigned len      = nargs >= 3 ? lua_tounsigned(L, 3) : 0x4300;
    destaddr = addr_remap(destaddr);
    const char *file_name = nargs >= 4 ? lua_tostring(L, 4) : NULL;
    uint8_t *src_mem = NULL;
    if (file_name != NULL) {
        char full_filename[PATH_MAX];
        if (strstr(file_name, ".p8") == NULL && strstr(file_name, ".P8") == NULL) {
            snprintf(full_filename, sizeof(full_filename), "%s.p8", file_name);
            file_name = full_filename;
        }
        char resolved_path[PATH_MAX];
        p8_show_io_icon(true);
        if (p8_resolve_relative_path(resolved_path, file_name, sizeof(resolved_path), false) < 0) {
            p8_show_io_icon(false);
            lua_pushinteger(L, 0);
            return 1;
        }
        int ret = parse_cart_file(resolved_path, m_temp_cart_memory, m_file_buffer, m_decompression_buffer, NULL, NULL);
        p8_show_io_icon(false);
        if (ret < 0) {
            lua_pushinteger(L, 0);
            return 1;
        }
        src_mem = m_temp_cart_memory;
    } else {
        src_mem = m_cart_memory;
    }
    if (destaddr >= 0 && destaddr + len <= 0x10000 && srcaddr >= 0 && srcaddr + len <= CART_MEMORY_SIZE)
        memcpy(m_memory + destaddr, src_mem + srcaddr, len);
    lua_pushinteger(L, len);
    return 1;
}

// ****************************************************************
// *** Math ***
// ****************************************************************

// rnd(max)
int rnd(lua_State *L)
{
    int is_table = 0;

    if (lua_gettop(L) >= 1)
    {
        if (lua_istable(L, 1))
        {
            is_table = 1;
        }
    }

    uint32_t hi = m_memory[MEMORY_RNG_STATE] | (m_memory[MEMORY_RNG_STATE + 1] << 8) |
                  (m_memory[MEMORY_RNG_STATE + 2] << 16) | (m_memory[MEMORY_RNG_STATE + 3] << 24);
    uint32_t lo = m_memory[MEMORY_RNG_STATE + 4] | (m_memory[MEMORY_RNG_STATE + 5] << 8) |
                  (m_memory[MEMORY_RNG_STATE + 6] << 16) | (m_memory[MEMORY_RNG_STATE + 7] << 24);

    hi = (hi << 16) | (hi >> 16);
    hi += lo;
    lo += hi;

    m_memory[MEMORY_RNG_STATE] = hi & 0xFF;
    m_memory[MEMORY_RNG_STATE + 1] = (hi >> 8) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 2] = (hi >> 16) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 3] = (hi >> 24) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 4] = lo & 0xFF;
    m_memory[MEMORY_RNG_STATE + 5] = (lo >> 8) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 6] = (lo >> 16) & 0xFF;
    m_memory[MEMORY_RNG_STATE + 7] = (lo >> 24) & 0xFF;

    if (is_table)
    {
        size_t len = lua_rawlen(L, 1);
        if (len > 0)
        {
            int index = ((hi >> 16) % len) + 1;
            lua_rawgeti(L, 1, index);
        }
        else
        {
            lua_pushnil(L);
        }
    }
    else
    {
        uint32_t max_fixed = (lua_gettop(L) >= 1) ? fix32_bits(lua_tonumber(L, 1)) : 0x10000;
        uint32_t result_fixed = (max_fixed == 0) ? 0 : (hi % max_fixed);
        lua_pushnumber(L, fix32_from_bits(result_fixed));
    }

    return 1;
}

// srand(val)
int _srand(lua_State *L)
{
    // Capture the current RNG state as the "previous seed" (lo word is the seed)
    uint32_t prev_lo = m_memory[MEMORY_RNG_STATE+4] | (m_memory[MEMORY_RNG_STATE+5] << 8) |
                       (m_memory[MEMORY_RNG_STATE+6] << 16) | (m_memory[MEMORY_RNG_STATE+7] << 24);

    // Use the full 32-bit raw fix32 representation so that srand(0x5b04.17cb)
    // seeds with 0x5b0417cb, not just the integer part 0x5b04.
    uint32_t n = (uint32_t)fix32_bits(lua_tonumber(L, 1));

    p8_seed_rng_state(n);

    lua_pushnumber(L, fix32_from_bits(prev_lo));
    return 1;
}

// ****************************************************************
// *** Cartridge data ***
// ****************************************************************

// cartdata(id)
int cartdata(lua_State *L)
{
    const char *id = lua_tostring(L, 1);
    bool success = p8_open_cartdata(id);
    lua_pushboolean(L, success);
    return 1;
}

// dget(index)
int dget(lua_State *L)
{
    unsigned index = lua_tounsigned(L, 1);

    lua_pushnumber(L, fix32_from_bits((m_memory[MEMORY_CARTDATA + index*4 + 3] << 24) | (m_memory[MEMORY_CARTDATA + index*4 + 2] << 16) | (m_memory[MEMORY_CARTDATA + index*4 + 1] << 8) | m_memory[MEMORY_CARTDATA + index*4]));

    return 1;
}

// dset(index, value)
int dset(lua_State *L)
{
    unsigned index = lua_tounsigned(L, 1);
    lua_Number val_num = lua_tonumber(L, 2);
    uint32_t value = fix32_bits(val_num);

    m_memory[MEMORY_CARTDATA + index*4] = value;
    m_memory[MEMORY_CARTDATA + index*4 + 1] = value >> 8;
    m_memory[MEMORY_CARTDATA + index*4 + 2] = value >> 16;
    m_memory[MEMORY_CARTDATA + index*4 + 3] = value >> 24;

    p8_delayed_flush_cartdata();

    lua_pushnumber(L, val_num);
    return 1;
}

// ****************************************************************
// *** Coroutines ***
// ****************************************************************

// cocreate(func)
// coresume(cor)
// costatus(cor)
// yield()

// ****************************************************************
// *** Values and objects ***
// ****************************************************************

// setmetatable(tbl, metatbl)
// getmetatable(tbl)
// type(v)
// sub(str, from, [to])
int sub(lua_State *L)
{
    const char *str = lua_tostring(L, 1);
    int start = lua_tointeger(L, 2);
    int end = lua_to_or_default(L, integer, 3, -1);
    int str_len = strlen(str);

    if (start < 1) start = 1;
    if (start > str_len + 1) start = str_len + 1;

    if (end == -1)
    {
        int len = str_len - start + 1;
        if (len < 0) len = 0;
        lua_pushlstring(L, str + start - 1, len);
        return 1;
    }

    if (end < 0) end = str_len + end + 1;
    if (end < 1) end = 1;
    if (end > str_len) end = str_len;

    int len = end - start + 1;
    if (len < 0) len = 0;
    lua_pushlstring(L, str + start - 1, len);

    return 1;
}

// ****************************************************************
// *** Time ***
// ****************************************************************

// time()
int _time(lua_State *L)
{
    lua_pushnumber(L, fix32_from_double((double)m_frames / (double)m_fps));

    return 1;
}

// ****************************************************************
// *** System ***
// ****************************************************************

// menuitem(index, [label, callback])
int menuitem(lua_State *L)
{
    int index = luaL_checkinteger(L, 1);

    if (index < 1 || index > MAX_CUSTOM_MENUITEMS)
        return 0;

    /* menuitem(i) with no label removes the item */
    if (lua_gettop(L) < 2 || lua_isnil(L, 2)) {
        p8_menuitem_clear(index);
        return 0;
    }

    const char *label = lua_tostring(L, 2); /* may be nil/non-string */

    if (lua_gettop(L) >= 3 && lua_isfunction(L, 3)) {
        // Full set: update label and callback
        lua_pushvalue(L, 3);
        int callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        p8_menuitem_set(index, label ? label : "", callback_ref);
    } else {
        // Label-only update: preserve existing callback
        p8_menuitem_set_label(index, label ? label : "");
    }
    return 0;
}

// extcmd(cmd)
int extcmd(lua_State *L)
{
    const char *cmd = luaL_checkstring(L, 1);

    if (strcmp(cmd, "pause") == 0) {
        p8_show_pause_menu();
    } else if (strcmp(cmd, "reset") == 0) {
        p8_restart();
    } else if (strcmp(cmd, "shutdown") == 0) {
        p8_abort();
    } else {
        fprintf(stderr, "unsupported extcmd command: %s\n", cmd);
    }

    return 0;
}

int reset(lua_State *L)
{
    p8_reset();

    return 0;
}

int run(lua_State *L)
{
    const char *param = (lua_gettop(L) >= 1) ? lua_tostring(L, 1) : "";
    strtcpy(m_param_string, param, sizeof(m_param_string));

    return p8_run();
}

// load(filename [,breadcrumb] [,param])
int _load(lua_State *L)
{
    if (access(".", F_OK) == -1 && errno != EACCES)
        luaL_error(L, "load() requires a filesystem");

    int nargs = lua_gettop(L);

    if (nargs < 1)
        luaL_error(L, "load() requires at least 1 argument");

    const char *filename = lua_tostring(L, 1);
    if (!filename)
        luaL_error(L, "load() filename must be a string");

    char full_filename[PATH_MAX];
    char resolved_path[PATH_MAX];
    const char *bbs_cart_id = NULL;

    if (strstr(filename, ".p8") == NULL && strstr(filename, ".P8") == NULL) {
        snprintf(full_filename, sizeof(full_filename), "%s.p8", filename);
        filename = full_filename;
    }

    if (p8_resolve_relative_path(resolved_path, filename, sizeof(resolved_path), false) < 0) {
        luaL_error(L, "out of memory");
    }

    if (p8_resolve_relative_path(resolved_path, filename, sizeof(resolved_path), false) < 0) {
        luaL_error(L, "out of memory");
    }

    if (access(resolved_path, F_OK) != 0) {
        fprintf(stderr, "load: could not find cart %s\n", filename);
        m_load_result = -1;
        if (p8_is_cart_running()) {
            lua_pushboolean(L, 0);
            lua_pushstring(L, "could not find cart");
            return 2;
        } else {
            return luaL_error(L, "could not find cart");
        }
    }

    const char *breadcrumb = NULL;
    if (nargs >= 2)
        breadcrumb = lua_tostring(L, 2);

    const char *param = NULL;
    if (nargs >= 3)
        param = lua_tostring(L, 3);

    int ret = p8_load(resolved_path, param, bbs_cart_id, breadcrumb);
    if (ret != 0) {
        m_load_result = -1;
        if (p8_is_cart_running()) {
            lua_pushboolean(L, 0);
            lua_pushstring(L, "could not load cart");
            return 2;
        } else {
            return luaL_error(L, "could not load cart");
        }
    }

    m_load_result = 1;

    if (p8_is_cart_running()) {
        return p8_run();
    } else {
        lua_pushboolean(L, 1);
        return 1;
    }
}

// ****************************************************************
// *** Debugging ***
// ****************************************************************

// assert(cond, [message])
// printh(str, [filename], [overwrite])
int printh(lua_State *L)
{
    const char *str = NULL;
    const char *filename = NULL;
    bool overwrite = false;

    if (lua_gettop(L) >= 1)
        str = lua_tostring(L, 1);
    if (lua_gettop(L) >= 2)
        filename = lua_tostring(L, 2);
    if (lua_gettop(L) >= 3)
        overwrite = lua_toboolean(L, 3);

    if (!str) str = "";

    if (filename && strcmp(filename, "@clip") == 0) {
        strtcpy(m_clipboard, str, sizeof(m_clipboard));
        lua_pushboolean(L, 1);
        return 1;
    }

    FILE *f = NULL;
    if (!filename) {
        f = stdout;
    } else {
        f = fopen(filename, overwrite ? "w" : "a");
        if (!f) {
            lua_pushboolean(L, 0);
            return 1;
        }
    }

    fprintf(f, "%s\n", str);
    if (f != stdout)
        fclose(f);

    lua_pushboolean(L, 1);
    return 1;
}

// stat(n)
int lua_stat(lua_State *L)
{
    int n = lua_tointeger(L, 1);

    switch (n)
    {
    case STAT_MEM_USAGE: {
        lua_gc(L, LUA_GCCOLLECT, 0);
        int kb = lua_gc(L, LUA_GCCOUNT, 0);
        lua_pushnumber(L, fix32_from_int(kb));
        break;
    }
    case STAT_RAW_GC: {
        int kb = lua_gc(L, LUA_GCCOUNT, 0);
        lua_pushnumber(L, fix32_from_int(kb));
        break;
    }
    case STAT_CPU_USAGE:
    case STAT_SYSTEM_CPU_USAGE: {
        unsigned elapsed_time = p8_elapsed_time();
        const unsigned target_frame_time = 1000 / m_fps;
        float f = (float)elapsed_time / (float)target_frame_time;
        lua_pushnumber(L, fix32_from_double(f));
        break;
    }
    case STAT_CURRENT_DISPLAY:
        lua_pushinteger(L, 0);
        break;
    case STAT_CLIPBOARD:
        lua_pushstring(L, m_clipboard);
        break;
    case STAT_VERSION:
        lua_pushnumber(L, fix32_from_double(0.43));
        break;
    case STAT_PARAM:
        lua_pushstring(L, m_param_string);
        break;
    case STAT_FRAMERATE:
        lua_pushinteger(L, m_fps);
        break;
    case STAT_TARGET_FRAMERATE:
        lua_pushinteger(L, m_fps);
        break;
    case STAT_NUM_DISPLAYS:
        lua_pushinteger(L, 1);
        break;
    case STAT_PAUSE_MENU_X1:
        lua_pushinteger(L, 0);
        break;
    case STAT_PAUSE_MENU_Y1:
        lua_pushinteger(L, 0);
        break;
    case STAT_PAUSE_MENU_X2:
        lua_pushinteger(L, P8_WIDTH);
        break;
    case STAT_PAUSE_MENU_Y2:
        lua_pushinteger(L, P8_HEIGHT);
        break;
    case STAT_RAW_KEYBOARD: {
        int scancode = lua_tointeger(L, 2);
        lua_pushboolean(L, p8_is_key_down(scancode));
        break;
    }
    case STAT_KEY_PRESSED:
        lua_pushboolean(L, p8_has_pending_keypress());
        break;
    case STAT_KEY_NAME: {
        uint8_t keypress = 0;
        p8_get_next_keypress(NULL, &keypress, NULL);
        char s[2] = {(char)keypress, '\0'};
        lua_pushstring(L, s);
        break;
    }
    case STAT_MOUSE_X:
        lua_pushinteger(L, m_mouse_x);
        break;
    case STAT_MOUSE_Y:
        lua_pushinteger(L, m_mouse_y);
        break;
    case STAT_MOUSE_BUTTONS:
        lua_pushinteger(L, m_mouse_buttons);
        break;
    case STAT_MOUSE_WHEEL:
        lua_pushinteger(L, m_mouse_wheel);
        break;
    case STAT_MOUSE_XREL:
        lua_pushinteger(L, m_mouse_xrel);
        break;
    case STAT_MOUSE_YREL:
        lua_pushinteger(L, m_mouse_yrel);
        break;
    case STAT_YEAR:
    case STAT_MONTH:
    case STAT_DAY:
    case STAT_HOUR:
    case STAT_MINUTE:
    case STAT_SECOND: {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        switch (n)
        {
        case STAT_YEAR:
            lua_pushinteger(L, tm->tm_year + 1900);
            break;
        case STAT_MONTH:
            lua_pushinteger(L, tm->tm_mon + 1);
            break;
        case STAT_DAY:
            lua_pushinteger(L, tm->tm_mday);
            break;
        case STAT_HOUR:
            lua_pushinteger(L, tm->tm_hour);
            break;
        case STAT_MINUTE:
            lua_pushinteger(L, tm->tm_min);
            break;
        case STAT_SECOND:
            lua_pushinteger(L, tm->tm_sec);
            break;
        }
        break;
    }
    case STAT_YEAR_UTC:
    case STAT_MONTH_UTC:
    case STAT_DAY_UTC:
    case STAT_HOUR_UTC:
    case STAT_MINUTE_UTC:
    case STAT_SECOND_UTC: {
        time_t t = time(NULL);
        struct tm *tm = gmtime(&t);
        switch (n)
        {
        case STAT_YEAR_UTC:
            lua_pushinteger(L, tm->tm_year + 1900);
            break;
        case STAT_MONTH_UTC:
            lua_pushinteger(L, tm->tm_mon + 1);
            break;
        case STAT_DAY_UTC:
            lua_pushinteger(L, tm->tm_mday);
            break;
        case STAT_HOUR_UTC:
            lua_pushinteger(L, tm->tm_hour);
            break;
        case STAT_MINUTE_UTC:
            lua_pushinteger(L, tm->tm_min);
            break;
        case STAT_SECOND_UTC:
            lua_pushinteger(L, tm->tm_sec);
            break;
        }
        break;
    }
    case STAT_PCM_BUFFER_SIZE:
        lua_pushinteger(L, audio_pcm_buffered());
        break;
    case STAT_PCM_APP_BUFFER:
        lua_pushinteger(L, audio_pcm_app_buffer());
        break;
    case STAT_BREADCRUMB:
        lua_pushstring(L, m_breadcrumb);
        break;
    case STAT_BBS_CART_ID:
        lua_pushstring(L, "");
        break;
    case STAT_LOAD_RESULT:
        lua_pushinteger(L, m_load_result);
        break;
    case STAT_CURRENT_PATH:
        lua_pushstring(L, m_current_cart_dir[0] ? m_current_cart_dir : ".");
        break;
    default:
        if (n == 57) {
            lua_pushboolean(L, audio_stat(n) != 0);
        } else if ((n >= 46 && n <= 57) || (n >= 16 && n <= 26)) {
            lua_pushinteger(L, audio_stat(n));
        } else {
            lua_pushinteger(L, 0);
        }
        break;
    }

    return 1;
}

// stop([message,] [x,] [y,] [col])
int _stop(lua_State *L)
{
    int nargs = lua_gettop(L);
    if (nargs >= 1 && !lua_isnil(L, 1)) {
        size_t len;
        const char *str = lua_tolstring(L, 1, &len);

        if (str) {
            if (nargs >= 3) {
                int x = lua_tointeger(L, 2);
                int y = lua_tointeger(L, 3);
                int col = nargs >= 4 ? lua_tointeger(L, 4) : pencolor_get();
                int right;
                draw_text(str, len, x, y, col, x, true, &x, &y, &right);
                cursor_set(x, y, -1);
            } else {
                int x, y;
                cursor_get(&x, &y);
                int col = nargs >= 2 ? lua_tointeger(L, 2) : pencolor_get();
                int right;
                draw_text(str, len, x, y, col, left_margin_get(), false, &x, &y, &right);
                cursor_set(x, y, -1);
            }
            p8_render();
        }
    }
    p8_abort();
}

// trace([coroutine,] [message,] [skip])
// Returns a description of the current call stack as a string.
// Mirrors debug.traceback() with PICO-8's default skip of 1.
int _trace(lua_State *L)
{
    int arg = 0;
    lua_State *L1 = L;

    if (lua_isthread(L, 1)) {
        L1 = lua_tothread(L, 1);
        arg = 1;
    }

    const char *msg = lua_isnoneornil(L, arg + 1) ? NULL : lua_tostring(L, arg + 1);
    int level = lua_isnoneornil(L, arg + 2) ? 1 : lua_tointeger(L, arg + 2);

    luaL_traceback(L, L1, msg, level);
    return 1;
}

// ****************************************************************
// *** Misc ***
// ****************************************************************

// ****************************************************************
// ****************************************************************

int warning(lua_State *L)
{
    const char *msg = lua_tostring(L, 1);

    printf("WARNING: %s\r\n", msg);

    return 0;
}

int set_fps(lua_State *L)
{
    int fps = lua_tointeger(L, 1);
    m_fps = MAX(0, fps);

    return 0;
}

int get_mouse_x(lua_State *L)
{
    lua_pushinteger(L, m_mouse_x);

    return 1;
}

int get_mouse_y(lua_State *L)
{
    lua_pushinteger(L, m_mouse_y);

    return 1;
}

// serial(channel, address, [length])
int serial(lua_State *L)
{
    int channel = lua_tointeger(L, 1);
    uint32_t address = addr_remap(lua_tounsigned(L, 2));
    uint32_t length = lua_gettop(L) >= 3 ? lua_tounsigned(L, 3) : 1;
    if (length < 0)
        length = 0;

    switch (channel) {
    case 0x0ff: {
        usleep(length);
        break;
    }
    case 0x804: {
        if (address >= MEMORY_SIZE)
            length = 0;
        else if (length > MEMORY_SIZE - address)
            length = MEMORY_SIZE - address;

        if (length > 0) {
            size_t bytes_read = fread(m_memory + address, 1, length, stdin);
            (void)bytes_read;
        }
        break;
    }
    case 0x805: {
        if (address >= MEMORY_SIZE)
            length = 0;
        else if (length > MEMORY_SIZE - address)
            length = MEMORY_SIZE - address;

        if (length > 0)
            fwrite(m_memory + address, 1, length, stdout);

        fflush(stdout);
        break;
    }
    case 0x808:
        // PCM audio output
#ifdef ENABLE_AUDIO
        audio_pcm_write(address, length);
#endif
        break;
    }

    return 0;
}

// ****************************************************************
// *** Filesystem ***
// ****************************************************************

// save([filename])
// Save the current cartridge to disk.
int save(lua_State *L)
{
    const char *filename = lua_gettop(L) >= 1 ? lua_tostring(L, 1) : NULL;

    char resolved[PATH_MAX];
    if (filename) {
        if (p8_resolve_relative_path(resolved, filename, sizeof(resolved), true) < 0) {
            luaL_error(L, "out of memory");
            return 0;
        }
    } else {
        if (!m_current_cart_file_name[0]) {
            lua_pushboolean(L, 0);
            return 1;
        }
        strtcpy(resolved, m_current_cart_file_name, sizeof(resolved));
    }

    p8_show_io_icon(true);
    int ret = write_cart_p8(resolved, m_lua_script, m_cart_memory);
    p8_show_io_icon(false);

    if (ret != 0) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    return 1;
}

// ls([directory])
// List .p8 and .p8.png files in the given directory (relative to current_cart_dir).
// Directories are returned with a trailing '/'.
// Returns nil when called from a BBS cart.
int ls(lua_State *L)
{
    const char *path = NULL;
    char resolved[PATH_MAX];

    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        const char *arg = lua_tostring(L, 1);
        if (arg) {
            if (p8_resolve_relative_path(resolved, arg, sizeof(resolved), false) < 0) {
                luaL_error(L, "Path too long");
                return 0;
            }
            path = resolved;
        }
    }

    if (!path)
        path = m_current_cart_dir[0] ? m_current_cart_dir : ".";

    DIR *dir = opendir(path);
    if (!dir)
        return 0; /* nil */

    lua_newtable(L);
    int idx = 1;

    struct dirent *de;
    char full_path[PATH_MAX];
    while ((de = readdir(dir)) != NULL) {
        char *name = de->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        /* Build full path for stat */
        if (p8_make_full_path(full_path, sizeof(full_path), path, name) < 0)
            continue;

        struct stat st;
        bool is_dir = false;
        if (stat(full_path, &st) == 0)
            is_dir = S_ISDIR(st.st_mode);

        size_t name_len = strlen(name);
        if (is_dir) {
            /* Append trailing '/' */
            if (name_len < sizeof(de->d_name) - 2) {
                name[name_len] = '/';
                name[name_len + 1] = '\0';
                lua_pushstring(L, name);
                lua_rawseti(L, -2, idx++);
            }
        } else {
            /* Only include .p8 and .p8.png files */
            bool is_p8 = (name_len > 3 && strcmp(name + name_len - 3, ".p8") == 0) ||
                         (name_len > 7 && strcmp(name + name_len - 7, ".p8.png") == 0);
            if (is_p8) {
                lua_pushstring(L, name);
                lua_rawseti(L, -2, idx++);
            }
        }
    }
    closedir(dir);
    return 1;
}

// reboot()
// Reboot the machine / start a new project.
int lua_reboot(lua_State *L)
{
    (void)L;
    p8_abort();
    return 0;
}

// cd([directory])
// Change the current cartridge directory.
int cd(lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_isnil(L, 1))
        return 0;

    const char *dir = lua_tostring(L, 1);
    if (!dir)
        return 0;

    char new_dir[PATH_MAX];
    if (p8_resolve_relative_path(new_dir, dir, sizeof(new_dir), false) < 0) {
        luaL_error(L, "Path too long");
        return 0;
    }

    struct stat st;
    if (stat(new_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        luaL_error(L, "%s: not a directory", dir);
        return 0;
    }

    strtcpy(m_current_cart_dir, new_dir, sizeof(m_current_cart_dir));
    return 0;
}

// mkdir(directory)
// Create a directory.
int lua_mkdir(lua_State *L)
{
    const char *dir = lua_gettop(L) >= 1 ? lua_tostring(L, 1) : NULL;
    if (!dir) {
        lua_pushboolean(L, 0);
        return 1;
    }

    char resolved[PATH_MAX];
    if (p8_resolve_relative_path(resolved, dir, sizeof(resolved), true) < 0) {
        luaL_error(L, "Path too long");
        return 0;
    }

    int ret = MKDIR(resolved);

    if (ret != 0 && errno != EEXIST) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    return 1;
}

void lua_register_functions(lua_State *L)
{
    // ****************************************************************
    // *** Graphics ***
    // ****************************************************************
    lua_register(L, "camera", camera);
    lua_register(L, "circ", circ);
    lua_register(L, "circfill", circfill);
    lua_register(L, "clip", clip);
    lua_register(L, "cls", cls);
    lua_register(L, "color", color);
    lua_register(L, "cursor", cursor);
    lua_register(L, "fget", fget);
    lua_register(L, "fillp", fillp);
    lua_register(L, "flip", flip);
    lua_register(L, "holdframe", holdframe);
    lua_register(L, "fset", fset);
    lua_register(L, "line", line);
    lua_register(L, "oval", oval);
    lua_register(L, "ovalfill", ovalfill);
    lua_register(L, "pal", pal);
    lua_register(L, "palt", palt);
    lua_register(L, "pget", pget);
    lua_register(L, "print", print);
    lua_register(L, "pset", pset);
    lua_register(L, "rect", rect);
    lua_register(L, "rectfill", rectfill);
    lua_register(L, "rrect", rrect);
    lua_register(L, "rrectfill", rrectfill);
    lua_register(L, "sget", sget);
    lua_register(L, "spr", spr);
    lua_register(L, "sset", sset);
    lua_register(L, "sspr", sspr);
    lua_register(L, "tline", tline);
    // ****************************************************************
    // *** Tables ***
    // ****************************************************************
    // lua_register(L, "add", add);
    // lua_register(L, "all", all);
    // lua_register(L, "count", count);
    // lua_register(L, "del", del);
    // lua_register(L, "foreach", foreach);
    // lua_register(L, "pairs", pairs);
    // ****************************************************************
    // *** Input ***
    // ****************************************************************
    lua_register(L, "btn", btn);
    lua_register(L, "btnp", btnp);
    lua_register(L, "_update_buttons", _update_buttons);
    // ****************************************************************
    // *** Sound ***
    // ****************************************************************
    lua_register(L, "music", music);
    lua_register(L, "sfx", sfx);
    // ****************************************************************
    // *** Map ***
    // ****************************************************************
    lua_register(L, "map", map);
    lua_register(L, "mget", mget);
    lua_register(L, "mset", mset);
    // ****************************************************************
    // *** Memory ***
    // ****************************************************************
    lua_register(L, "cstore", cstore);
    lua_register(L, "memcpy", _memcpy);
    lua_register(L, "memset", _memset);
    lua_register(L, "peek", peek);
    lua_register(L, "peek2", peek2);
    lua_register(L, "peek4", peek4);
    lua_register(L, "poke", poke);
    lua_register(L, "poke2", poke2);
    lua_register(L, "poke4", poke4);
    lua_register(L, "reload", reload);
    // ****************************************************************
    // *** Math ***
    // ****************************************************************
    // lua_register(L, "abs", _abs); // in lpico8lib.c
    // lua_register(L, "atan2", _atan2); // in lpico8lib.c
    // lua_register(L, "band", band); // in lpico8lib.c
    // lua_register(L, "bnot", bnot); // in lpico8lib.c
    // lua_register(L, "bor", bor); // in lpico8lib.c
    // lua_register(L, "bxor", bxor); // in lpico8lib.c
    // lua_register(L, "ceil", _ceil); // in lpico8lib.c
    // lua_register(L, "cos", _cos); // in lpico8lib.c
    // lua_register(L, "flr", flr); // in lpico8lib.c
    // lua_register(L, "lshr", lshr); // in lpico8lib.c
    // lua_register(L, "max", max); // in lpico8lib.c
    // lua_register(L, "mid", mid); // in lpico8lib.c
    // lua_register(L, "min", min); // in lpico8lib.c
    lua_register(L, "rnd", rnd);
    // lua_register(L, "rotl", rotl); // in lpico8lib.c
    // lua_register(L, "rotr", rotr); // in lpico8lib.c
    // lua_register(L, "sgn", sgn); // in lpico8lib.c
    // lua_register(L, "shl", shl); // in lpico8lib.c
    // lua_register(L, "shr", shr); // in lpico8lib.c
    // lua_register(L, "sin", _sin); // in lpico8lib.c
    // lua_register(L, "sqrt", _sqrt); // in lpico8lib.c
    lua_register(L, "srand", _srand);
    // ****************************************************************
    // *** Cartridge data ***
    // ****************************************************************
    lua_register(L, "cartdata", cartdata);
    lua_register(L, "dget", dget);
    lua_register(L, "dset", dset);
    // ****************************************************************
    // *** Coroutines ***
    // ****************************************************************
    // lua_register(L, "cocreate", cocreate);
    // lua_register(L, "coresume", coresume);
    // lua_register(L, "costatus", costatus);
    // lua_register(L, "yield", yield);
    // ****************************************************************
    // *** Values and objects ***
    // ****************************************************************
    // lua_register(L, "chr", chr); // in lpico8lib.c
    // lua_register(L, "ord", ord); // in lpico8lib.c
    // lua_register(L, "setmetatable", setmetatable);
    // lua_register(L, "getmetatable", getmetatable);
    // lua_register(L, "type", type);
    lua_register(L, "sub", sub);
    // lua_register(L, "tonum", tonum);
    // lua_register(L, "tostr", tostr); // in lpico8lib.c
    // ****************************************************************
    // *** Time ***
    // ****************************************************************
    lua_register(L, "t", _time);
    lua_register(L, "time", _time);
    // ****************************************************************
    // *** System ***
    // ****************************************************************
    lua_register(L, "menuitem", menuitem);
    lua_register(L, "extcmd", extcmd);
    lua_register(L, "load", _load);
    lua_register(L, "reset", reset);
    lua_register(L, "run", run);
    lua_register(L, "serial", serial);
    lua_register(L, "_set_fps", set_fps);
    lua_register(L, "save", save);
    lua_register(L, "ls", ls);
    lua_register(L, "reboot", lua_reboot);
    lua_register(L, "cd", cd);
    lua_register(L, "mkdir", lua_mkdir);
    // ****************************************************************
    // *** Debugging ***
    // ****************************************************************
    // lua_register(L, "assert", assert);
    lua_register(L, "printh", printh);
    lua_register(L, "stat", lua_stat);
    lua_register(L, "stop", _stop);
    lua_register(L, "trace", _trace);
    // ****************************************************************
    // *** Misc ***
    // ****************************************************************

    // ****************************************************************
    // *** P8SCII glyphs ***
    // ****************************************************************
    lua_pushinteger(L, 0); lua_setglobal(L, "\x8b");  // 139 ⬅️ left
    lua_pushinteger(L, 1); lua_setglobal(L, "\x91");  // 145 ➡️ right
    lua_pushinteger(L, 2); lua_setglobal(L, "\x94");  // 148 ⬆️ up
    lua_pushinteger(L, 3); lua_setglobal(L, "\x83");  // 131 ⬇️ down
    lua_pushinteger(L, 4); lua_setglobal(L, "\x8e");  // 142 🅾️ O/Z
    lua_pushinteger(L, 5); lua_setglobal(L, "\x97");  // 151 ❎ X

    lua_pushnumber(L, fix32_from_bits(0x00008000)); lua_setglobal(L, "\x80");  // 128 █
    lua_pushnumber(L, fix32_from_bits(0x5a5a8000)); lua_setglobal(L, "\x81");  // 129 ▒
    lua_pushnumber(L, fix32_from_bits(0x511f8000)); lua_setglobal(L, "\x82");  // 130 🐱
    lua_pushnumber(L, fix32_from_bits(0x7d7d8000)); lua_setglobal(L, "\x84");  // 132 ░
    lua_pushnumber(L, fix32_from_bits(0xb81d8000)); lua_setglobal(L, "\x85");  // 133 ✽
    lua_pushnumber(L, fix32_from_bits(0xf99f8000)); lua_setglobal(L, "\x86");  // 134 ●
    lua_pushnumber(L, fix32_from_bits(0x51bf8000)); lua_setglobal(L, "\x87");  // 135 ♥
    lua_pushnumber(L, fix32_from_bits(0xb5bf8000)); lua_setglobal(L, "\x88");  // 136 ☉
    lua_pushnumber(L, fix32_from_bits(0x999f8000)); lua_setglobal(L, "\x89");  // 137 웃
    lua_pushnumber(L, fix32_from_bits(0xb11f8000)); lua_setglobal(L, "\x8a");  // 138 ⌂
    lua_pushnumber(L, fix32_from_bits(0xa0e08000)); lua_setglobal(L, "\x8c");  // 140 😐
    lua_pushnumber(L, fix32_from_bits(0x9b3f8000)); lua_setglobal(L, "\x8d");  // 141 ♪
    lua_pushnumber(L, fix32_from_bits(0xb1bf8000)); lua_setglobal(L, "\x8f");  // 143 ◆
    lua_pushnumber(L, fix32_from_bits(0xf5ff8000)); lua_setglobal(L, "\x90");  // 144 …
    lua_pushnumber(L, fix32_from_bits(0xb15f8000)); lua_setglobal(L, "\x92");  // 146 ★
    lua_pushnumber(L, fix32_from_bits(0x1b1f8000)); lua_setglobal(L, "\x93");  // 147 ⧗
    lua_pushnumber(L, fix32_from_bits(0xf5bf8000)); lua_setglobal(L, "\x95");  // 149 ˇ
    lua_pushnumber(L, fix32_from_bits(0x7adf8000)); lua_setglobal(L, "\x96");  // 150 ∧
    lua_pushnumber(L, fix32_from_bits(0x0f0f8000)); lua_setglobal(L, "\x98");  // 152 ▤
    lua_pushnumber(L, fix32_from_bits(0x55558000)); lua_setglobal(L, "\x99");  // 153 ▥
}

static void lua_event_pump_hook(lua_State *L, lua_Debug *ar)
{
    (void)L;
    (void)ar;
    p8_pump_events();
    p8_check_for_pause();
}

int lua_load_api()
{
    if (!L)
    {
        L = luaL_newstate();
    }

    luaL_openlibs(L);

    lua_register_functions(L);

    int ret = luaL_dostring(L, lua_api_string);
    if (ret != 0)
        return ret;
    lua_setpico8memory(L, m_memory);

    // Set debug hook to pump events every ~3000 instructions
    lua_sethook(L, lua_event_pump_hook, LUA_MASKCOUNT, 3000);

    return 0;
}

int lua_shutdown_api()
{
    if (L) {
        p8_menuitem_reset_all();
        lua_close(L);
        L = NULL;
    }
    return 0;
}

static void print_script_context(const char *filename, int lineno)
{
    (void) filename;
    if (lineno <= 0)
        return;
    int lo = lineno - 3, hi = lineno + 3;
    if (lo < 1) lo = 1;
    const char *p = m_lua_script;
    int cur = 1;
    while (*p && cur < lo) {
        if (*p++ == '\n') cur++;
    }
    while (*p && cur <= hi) {
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        printf("%s %4d: %.*s\n",
               cur == lineno ? ">>>" : "   ",
               cur, (int)(eol - p), p);
        if (*eol) eol++;
        p = eol;
        cur++;
    }
}

void lua_print_error()
{
    const char *err_type = NULL;
    char err_msg[96] = {0};
    const char *filename = NULL;
    int lineno = -1;
    lua_get_error(&err_type, err_msg, sizeof(err_msg), &filename, &lineno);
    if (err_type) {
        if (lineno > 0)
            fprintf(stderr, "%s line %d\n", err_type, lineno);
        else
            fprintf(stderr, "%s\n", err_type);
    } else if (lineno > 0) {
        fprintf(stderr, "line %d\n", lineno);
    }
    if (err_msg[0])
        fprintf(stderr, "%s\n", err_msg);
    if (!err_type && !err_msg[0])
        fprintf(stderr, "unknown error\n");
    if (lineno != -1)
        print_script_context(filename, lineno);
}

int lua_init_script(const char *file_name, const char *script)
{
    if (!L)
        L = luaL_newstate();

    char temp_file_name[PATH_MAX + 1];
    temp_file_name[0] = '@';
    if (file_name)
        strtcpy(temp_file_name + 1, file_name, PATH_MAX);
    else
        temp_file_name[1] = '\0';

    lua_settop(L, 0);

    size_t script_len = strnlen(script, LUA_SCRIPT_SIZE);
    m_status = luaL_loadbuffer(L, script, script_len, temp_file_name);

    if (m_status)
        return m_status;

    m_status = lua_pcall(L, 0, 0, 0);

    if (m_status)
        return m_status;

    lua_getglobal(L, "_update");

    if (lua_isfunction(L, -1))
    {
        m_lua_update = lua_topointer(L, -1);
        m_fps = 30;
        lua_pop(L, 1);
    }

    lua_getglobal(L, "_update60");

    if (lua_isfunction(L, -1))
    {
        m_lua_update60 = lua_topointer(L, -1);
        m_fps = 60;
        lua_pop(L, 1);
    }

    lua_getglobal(L, "_draw");

    if (lua_isfunction(L, -1))
    {
        m_lua_draw = lua_topointer(L, -1);
        lua_pop(L, 1);
    }

    lua_getglobal(L, "_init");

    if (lua_isfunction(L, -1))
    {
        m_lua_init = lua_topointer(L, -1);
        lua_pop(L, 1);
    }

    return 0;
}

int lua_call_function(const char *name, int ret)
{
    lua_settop(L, 0);
    lua_getglobal(L, name);
    m_status = lua_pcall(L, 0, ret, 0);
    return m_status;
}

int lua_update()
{
    if (!m_lua_update && !m_lua_update60)
    {
        lua_getglobal(L, "_update");
        if (lua_isfunction(L, -1))
        {
            m_lua_update = lua_topointer(L, -1);
            m_fps = 30;
        }
        lua_pop(L, 1);

        lua_getglobal(L, "_update60");
        if (lua_isfunction(L, -1))
        {
            m_lua_update60 = lua_topointer(L, -1);
            m_fps = 60;
        }
        lua_pop(L, 1);
    }

    lua_settop(L, 0);
    m_status = 0;

    if (m_lua_update60)
        return lua_call_function("_update60", 0);
    else if (m_lua_update)
        return lua_call_function("_update", 0);
    return 0;
}

int lua_draw()
{
    if (!m_lua_draw)
    {
        lua_getglobal(L, "_draw");
        if (lua_isfunction(L, -1))
            m_lua_draw = lua_topointer(L, -1);
        lua_pop(L, 1);
    }

    lua_settop(L, 0);
    m_status = 0;

    if (m_lua_draw)
        return lua_call_function("_draw", 0);
    return 0;
}

bool lua_has_main_loop_callbacks()
{
    return (m_lua_update != NULL || m_lua_update60 != NULL || m_lua_draw != NULL);
}

int lua_init()
{
    m_status = 0;
    if (m_lua_init)
        return lua_call_function("_init", 0);
    return 0;
}

void lua_get_error(const char **err_type, char *err, int err_size, const char **filename, int *lineno)
{
    if (!L) {
        strtcpy(err, "no lua state", err_size);
        return;
    }

    switch (m_status ){
    case LUA_ERRSYNTAX:
        *err_type = "syntax error";
        break;
    case LUA_ERRMEM:
        *err_type = "out of memory";
        break;
    case LUA_ERRRUN:
        *err_type = "runtime error";
        break;
    case LUA_ERRERR:
        *err_type = "error in error handler";
        break;
    default:
        *err_type = "unknown error";
        break;
    }
    if (lua_gettop(L) > 0 && lua_isstring(L, -1)) {
        const char *err_msg = lua_tostring(L, -1);
        if (err_msg) {
            const char *colon = strrchr(err_msg, ':');
            if (colon)
                err_msg = colon + 2;
            const char *near_kw = strstr(err_msg, " near ");
            if (near_kw)
                *(char *)near_kw = '\0';
            if (strstr(err_msg, "syntax error") == NULL)
                strtcpy(err, err_msg, err_size);
        }

        if (lineno) {
            const char *message = lua_tostring(L, -1);
            // Extract line number from error string like "...:1292:"
            const char *colon = message;
            *lineno = 0;
            if (*colon == ':') {
                *lineno = atoi(colon + 1);
            } else {
                while (*colon) {
                    if (*colon != ']' && *(colon+1) == ':') {
                        *lineno = atoi(colon + 2);
                        if (*lineno > 0) break;
                    }
                    colon++;
                }
            }
        }
    }
}
