/**
 * Copyright (C) 2026 Chris January
 */

#ifndef P8_OVERLAY_HELPER_H
#define P8_OVERLAY_HELPER_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "p8_emu.h"
#include "pico_font.h"

/* Overlay clip region (global state) */
static int overlay_clip_x0 = 0;
static int overlay_clip_y0 = 0;
static int overlay_clip_x1 = P8_WIDTH;
static int overlay_clip_y1 = P8_HEIGHT;

static inline void overlay_clip_set(int x, int y, int w, int h)
{
    overlay_clip_x0 = x;
    overlay_clip_y0 = y;
    overlay_clip_x1 = x + w;
    overlay_clip_y1 = y + h;
}

static inline void overlay_clip_reset(void)
{
    overlay_clip_x0 = 0;
    overlay_clip_y0 = 0;
    overlay_clip_x1 = P8_WIDTH;
    overlay_clip_y1 = P8_HEIGHT;
}

static inline void overlay_clip_get(int *x, int *y, int *w, int *h)
{
    *x = overlay_clip_x0;
    *y = overlay_clip_y0;
    *w = overlay_clip_x1 - overlay_clip_x0;
    *h = overlay_clip_y1 - overlay_clip_y0;
}

static inline void overlay_draw_hline(int x0, int x1, int y, int col)
{
    if (y < overlay_clip_y0 || y >= overlay_clip_y1)
        return;

    if (x0 > x1) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }

    if (x0 < overlay_clip_x0) x0 = overlay_clip_x0;
    if (x1 >= overlay_clip_x1) x1 = overlay_clip_x1 - 1;

    uint8_t *dest = m_overlay_memory + ((x0 >> 1) + y * 64);


    // Handle odd start
    if (x0 & 1) {
        *dest = (col << 4) | (*dest & 0xF);
        dest++;
        x0++;
    }

    for (int x = x0; x < x1; x += 2)
        *dest++ = (col & 0xF) | (col << 4);

    if ((x1 & 1) == 0)
        *dest = (*dest & 0xF0) | (col & 0xF);
}

static inline void overlay_draw_vline(int x, int y0, int y1, int col)
{
    if (x < overlay_clip_x0 || x >= overlay_clip_x1)
        return;

    if (y0 > y1) {
        int tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    if (y0 < overlay_clip_y0) y0 = overlay_clip_y0;
    if (y1 >= overlay_clip_y1) y1 = overlay_clip_y1 - 1;

    uint8_t *dest = m_overlay_memory + (x >> 1) + y0 * 64;

    if ((x & 1) == 0) {
        for (int y = y0; y <= y1; y++) {
            *dest = (*dest & 0xF0) | (col & 0xF);
            dest += 64;
        }
    } else {
        for (int y = y0; y <= y1; y++) {
            *dest = (col << 4) | (*dest & 0xF);
            dest += 64;
        }
    }
}

static inline void overlay_draw_rect(int x0, int y0, int x1, int y1, int col)
{
    overlay_draw_hline(x0, x1, y0, col);
    overlay_draw_hline(x0, x1, y1, col);
    overlay_draw_vline(x0, y0, y1, col);
    overlay_draw_vline(x1, y0, y1, col);
}

static inline void overlay_draw_rectfill(int x0, int y0, int x1, int y1, int col)
{
    if (x0 > x1) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }

    if (y0 > y1) {
        int tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    if (x0 < overlay_clip_x0) x0 = overlay_clip_x0;
    if (y0 < overlay_clip_y0) y0 = overlay_clip_y0;
    if (x1 >= overlay_clip_x1) x1 = overlay_clip_x1 - 1;
    if (y1 >= overlay_clip_y1) y1 = overlay_clip_y1 - 1;

    if (x0 & 1) {
        uint8_t *dest = m_overlay_memory + ((x0 >> 1) + y0 * 64);
        for (int y = y0; y <= y1; y++) {
            *dest = (col << 4) | (*dest & 0xF);
            dest += 64;
        }
        x0++;
    }

    for (int y = y0; y <= y1; y++) {
        uint8_t *dest = m_overlay_memory + ((x0 >> 1) + y * 64);
        for (int x = x0; x < x1; x += 2)
            *dest++ = (col & 0xF) | (col << 4);
    }

    if ((x1 & 1) == 0) {
        uint8_t *dest = m_overlay_memory + ((x1 >> 1) + y0 * 64);
        for (int y = y0; y <= y1; y++) {
            *dest = (*dest & 0xF0) | (col & 0xF);
            dest += 64;
        }
    }
}

static inline int overlay_get_text_width(const char *text)
{
    int w = 0;
    for (const char *c = text; *c != '\0'; c++) {
        if ((uint8_t)*c >= 0x80)
            w += GLYPH_WIDTH * 2;
        else
            w += GLYPH_WIDTH;
    }
    return w;
}

static inline int overlay_get_text_width_len(const char *text, size_t len)
{
    int w = 0;
    for (size_t i = 0; i < len && text[i] != '\0'; i++) {
        if ((uint8_t)text[i] >= 0x80)
            w += GLYPH_WIDTH * 2;
        else
            w += GLYPH_WIDTH;
    }
    return w;
}

static inline void overlay_pixel(int x, int y, int col)
{
    if (x < overlay_clip_x0 || y < overlay_clip_y0 || x >= overlay_clip_x1 || y >= overlay_clip_y1)
        return;

    uint8_t *dest = m_overlay_memory + (x >> 1) + y * 64;
    if (x & 1)
        *dest = (col << 4) | (*dest & 0xF);
    else
        *dest = (*dest & 0xF0) | (col & 0xF);
}

static inline void overlay_draw_char(int n, int left, int top, int col)
{
    int sx = n % 16 * 8;
    int sy = n / 16 * 8;

    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            uint8_t index = font_map[(sx + x) / 2 + (sy + y) * 64];
            if (((sx + x) & 1) == 0)
                index = index & 0xF;
            else
                index = index >> 4;

            if (index == 7)
                overlay_pixel(left + x, top + y, col);
        }
    }
}

static inline void overlay_scroll_one_line()
{
    memmove(m_overlay_memory, m_overlay_memory + 64 * GLYPH_HEIGHT, 0x2000 - 64 * GLYPH_HEIGHT);
    memset(m_overlay_memory + 0x2000 - 64 * GLYPH_HEIGHT, 0, 64 * GLYPH_HEIGHT);
}

static inline int overlay_draw_text(const char *str, int x, int y, int col)
{
    int cursor_x = x;
    int cursor_y = y;
    for (const char *c = str; *c != '\0'; c++) {
        overlay_draw_char((uint8_t)*c, cursor_x, cursor_y, col);
        if ((uint8_t)*c >= 0x80)
            cursor_x += GLYPH_WIDTH * 2;
        else
            cursor_x += GLYPH_WIDTH;
        if (cursor_x >= P8_WIDTH) {
            cursor_x = 0;
            cursor_y += GLYPH_HEIGHT;
        }
        if (cursor_y > P8_HEIGHT - GLYPH_HEIGHT) {
            overlay_scroll_one_line();
            cursor_y -= GLYPH_HEIGHT;
        }
    }
    return cursor_x;
}

static inline void overlay_draw_simple_text(const char *str, int x, int y, int col)
{
    int cursor_x = x;
    for (const char *c = str; *c != '\0'; c++) {
        overlay_draw_char((uint8_t)*c, cursor_x, y, col);
        if ((uint8_t)*c >= 0x80)
            cursor_x += GLYPH_WIDTH * 2;
        else
            cursor_x += GLYPH_WIDTH;
    }
}

static inline void overlay_clear(void)
{
    memset(m_overlay_memory, OVERLAY_TRANSPARENT_COLOR, MEMORY_SCREEN_SIZE);
}

static inline void overlay_draw_icon(const uint8_t *icon, int x, int y)
{
    assert((x & 1) == 0);
    uint8_t *dest = m_overlay_memory + (x >> 1) + y * 64;
    for (int r=0;r<8;++r) {
        *dest++ = *icon++;
        *dest++ = *icon++;
        *dest++ = *icon++;
        *dest++ = *icon++;
        dest += 60;
    }
}

#endif /* P8_OVERLAY_HELPER_H */
