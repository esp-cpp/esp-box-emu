/**
 * Copyright (C) 2026 Chris January
 */

#include <stdbool.h>

#include "p8_options.h"
#include "p8_dialog.h"
#include "p8_emu.h"
#include "p8_overlay_helper.h"

/* Height of the keyboard reference card drawing area (in pixels) */
#define CONTROLS_CARD_HEIGHT 38

/* Draw the keyboard reference card for the controls dialog */
static void draw_controls_card(const p8_dialog_t *dialog, void *user_data, int x, int y, int width, int height)
{
    (void)dialog;
    (void)user_data;
    (void)height;

    int col_w = width / 2;
    int col2  = x + col_w;
    int ry    = y;

    /* Inverted header row: "player 1" left, "player 2" right */
    overlay_draw_rectfill(x, ry, x + col_w - GLYPH_WIDTH - 1, ry + GLYPH_HEIGHT - 1, DIALOG_BG_INVERTED);
    overlay_draw_simple_text("player 1", x + 1, ry, DIALOG_TEXT_INVERTED);
    overlay_draw_rectfill(col2, ry, col2 + col_w - 1, ry + GLYPH_HEIGHT - 1, DIALOG_BG_INVERTED);
    overlay_draw_simple_text("player 2", col2 + 1, ry, DIALOG_TEXT_INVERTED);
    ry += GLYPH_HEIGHT + 2;

    /* Movement row */
    overlay_draw_simple_text("arrows", x, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("move", x + 7 * GLYPH_WIDTH, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("s/d/e/f", col2, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("move", col2 + 9 * GLYPH_WIDTH, ry, DIALOG_TEXT_NORMAL);
    ry += GLYPH_HEIGHT + 1;

    /* Button 1 row */
    overlay_draw_simple_text("z,c,n", x, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("btn 1", x + 7 * GLYPH_WIDTH, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("tab/shft", col2, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("btn 1", col2 + 9 * GLYPH_WIDTH, ry, DIALOG_TEXT_NORMAL);
    ry += GLYPH_HEIGHT + 1;

    /* Button 2 row */
    overlay_draw_simple_text("x,m,v", x, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("btn 2", x + 7 * GLYPH_WIDTH, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("q,a", col2, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("btn 2", col2 + 9 * GLYPH_WIDTH, ry, DIALOG_TEXT_NORMAL);
    ry += GLYPH_HEIGHT + 2;

    /* Separator line */
    overlay_draw_hline(x, x + width - 1, ry, DIALOG_TEXT_NORMAL);
    ry += 2;

    /* Pause row (spans both columns) */
    overlay_draw_simple_text("p/enter", x, ry, DIALOG_TEXT_NORMAL);
    overlay_draw_simple_text("pause", x + 8 * GLYPH_WIDTH, ry, DIALOG_TEXT_NORMAL);
}

/* Show the keyboard controls reference card dialog */
void p8_show_controls_dialog(void)
{
    p8_dialog_control_t controls[] = {
        DIALOG_CUSTOM_CONTROL(110, CONTROLS_CARD_HEIGHT, draw_controls_card, NULL),
        DIALOG_SPACING(),
        DIALOG_BUTTONBAR_OK_ONLY()
    };

    p8_dialog_t dialog;
    p8_dialog_init(&dialog, "controls", controls,
                   sizeof(controls) / sizeof(controls[0]), 120);
    p8_dialog_run(&dialog);
    p8_dialog_cleanup(&dialog);
}
