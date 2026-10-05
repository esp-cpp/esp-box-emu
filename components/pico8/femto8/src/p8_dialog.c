/**
 * Copyright (C) 2026 Chris January
 *
 * Dialog API for femto8
 */

#include "p8_dialog.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_overlay_helper.h"
#include <string.h>
#include <ctype.h>

/* Control Dimensions */
#define CONTROL_PADDING_X         3   // Horizontal padding inside dialog
#define CONTROL_PADDING_Y         4   // Vertical padding at top/bottom
#define SEPARATOR_HEIGHT          2
#define SPACING_HEIGHT            2

#define MAX_DIALOG_NESTING        4     // Maximum depth of nested dialogs

/* Global Dialog State */
static int m_dialog_nest_count = 0;
static p8_dialog_t *m_dialog_stack[MAX_DIALOG_NESTING];
bool m_dialog_showing = false;

/* Helper function to get control height */
static int get_control_height(const p8_dialog_control_t *control)
{
    switch (control->type) {
        case DIALOG_SPACING:
            return SPACING_HEIGHT;
        case DIALOG_BUTTON:
        case DIALOG_BUTTONBAR:
        case DIALOG_CHECKBOX:
            return GLYPH_HEIGHT + 2;
        case DIALOG_INPUTBOX:
            return GLYPH_HEIGHT + 4;
        case DIALOG_LISTBOX: {
            int visible_lines = control->data.listbox.visible_lines;
            if (visible_lines == 0) {
                if (!control->data.listbox.draw_border)
                    return P8_HEIGHT - GLYPH_HEIGHT * 2;  // Max height without border

                // Auto-size based on item count (max 8 lines)
                visible_lines = control->data.listbox.item_count;
                if (visible_lines > 8)
                    visible_lines = 8;
            }
            return visible_lines * GLYPH_HEIGHT + (control->data.listbox.draw_border ? 2 : 0);
        }
        case DIALOG_CUSTOM:
            return control->data.custom.height;
        default:
            return GLYPH_HEIGHT;
    }
}

/* Helper function to check if dialog is in quick mode */
static bool is_quick_dialog_mode(const p8_dialog_t *dialog)
{
    int selectable_count = 0;
    int listbox_or_inputbox_count = 0;
    int buttonbar_count = 0;

    for (int i = 0; i < dialog->control_count; i++) {
        if (dialog->controls[i].selectable) {
            selectable_count++;
            if (dialog->controls[i].type == DIALOG_LISTBOX ||
                dialog->controls[i].type == DIALOG_INPUTBOX)
                listbox_or_inputbox_count++;
            if (dialog->controls[i].type == DIALOG_BUTTONBAR)
                buttonbar_count++;
        }
    }

    if (selectable_count == buttonbar_count + listbox_or_inputbox_count &&
        buttonbar_count == 1 && listbox_or_inputbox_count <= 1)
        return true;

    return false;
}

/* Initialize dialog structure */
void p8_dialog_init(p8_dialog_t *dialog,
                    const char *title,
                    p8_dialog_control_t *controls,
                    int count,
                    int width)
{
    memset(dialog, 0, sizeof(p8_dialog_t));

    dialog->title = title;
    dialog->controls = controls;
    dialog->control_count = count;
    dialog->width = width;
    dialog->padding = 1;
    dialog->draw_border = true;

    // Auto-size width if needed
    if (dialog->width == 0) {
        int max_width = 64;

        // Consider title
        if (title) {
            int title_width = overlay_get_text_width(title) + 2;
            if (title_width > max_width)
                max_width = title_width;
        }

        // Consider all control labels
        for (int i = 0; i < count; i++) {
            if (controls[i].label) {
                int label_width = overlay_get_text_width(controls[i].label) + 2;
                if (label_width > max_width)
                    max_width = label_width;
            }
            if (controls[i].type == DIALOG_CUSTOM && controls[i].data.custom.width > 0) {
                if (controls[i].data.custom.width > max_width)
                    max_width = controls[i].data.custom.width;
            }
        }

        // Add some margin for borders
        max_width += CONTROL_PADDING_X * 2;

        if (max_width < 64)
            max_width = 64;
        if (max_width > P8_WIDTH - 20)
            max_width = P8_WIDTH - 20;

        dialog->width = max_width;
    }

    // Calculate height
    int height = 0;

    if (dialog->draw_border) {
        height += 2 * CONTROL_PADDING_Y;  // Top and bottom padding
    }

    if (title) {
        height += GLYPH_HEIGHT + SEPARATOR_HEIGHT + CONTROL_PADDING_Y;  // Title + separator
    }

    for (int i = 0; i < count; i++) {
        height += get_control_height(&controls[i]);
        if (i < count - 1 && dialog->padding > 0) {
            height += dialog->padding;
        }
    }

    dialog->height = height;
    if (dialog->height > P8_HEIGHT) dialog->height = P8_HEIGHT;

    // Center dialog
    dialog->x = (P8_WIDTH - dialog->width) / 2;
    dialog->y = (P8_HEIGHT - dialog->height) / 2;

    // Find first selectable control
    dialog->focused_control = -1;
    for (int i = 0; i < count; i++) {
        if (controls[i].selectable) {
            dialog->focused_control = i;
            break;
        }
    }

    dialog->focused_subcontrol = 0;
    dialog->cursor_blink = 0;
}

/* Draw a control */
static void draw_control(const p8_dialog_t *dialog, int control_idx, int x, int y, int width, bool focused)
{
    const p8_dialog_control_t *control = &dialog->controls[control_idx];

    int bg_color = control->inverted ? DIALOG_BG_INVERTED : DIALOG_BG_NORMAL;
    int fg_color = control->inverted ? DIALOG_TEXT_INVERTED : DIALOG_TEXT_NORMAL;

    if (focused && !control->inverted) {
        bg_color = DIALOG_BG_HIGHLIGHT;
        fg_color = DIALOG_TEXT_HIGHLIGHT;
    }

    switch (control->type) {
        case DIALOG_LABEL: {
            // Fill background for inverted labels
            if (control->inverted)
                overlay_draw_rectfill(x, y, x + width - 1, y + GLYPH_HEIGHT - 1, bg_color);
            if (control->label)
                overlay_draw_simple_text(control->label, x + 1, y, fg_color);
            break;
        }

        case DIALOG_BUTTON: {
            int button_width = control->label ? overlay_get_text_width(control->label) + 2 : 20;
            if (focused)
                overlay_draw_rectfill(x, y, x + button_width - 1, y + GLYPH_HEIGHT + 1, bg_color);
            overlay_draw_simple_text(control->label, x + 1, y + 2, fg_color);
            break;
        }

        case DIALOG_MENUITEM: {
            if (focused)
                overlay_draw_rectfill(x, y, x + width - 1, y + GLYPH_HEIGHT, bg_color);
            overlay_draw_simple_text(control->label, x + 1, y + 1, fg_color);
            break;
        }

        case DIALOG_CHECKBOX: {
            if (focused)
                overlay_draw_rectfill(x, y, x + width - 1, y + GLYPH_HEIGHT + 1, bg_color);

            // Draw checkbox box
            int box_x = x + 1;
            int box_y = y + 1;
            overlay_draw_rect(box_x, box_y, box_x + 5, box_y + 5, fg_color);

            // Draw square if checked
            if (control->data.checkbox.value && *control->data.checkbox.value)
                overlay_draw_rectfill(box_x + 2, box_y + 2, box_x + 3, box_y + 3, fg_color);

            // Draw label
            if (control->label)
                overlay_draw_simple_text(control->label, box_x + 8, y + 2, fg_color);
            break;
        }

        case DIALOG_INPUTBOX: {
            // Draw border
            int border_color = focused ? DIALOG_BG_HIGHLIGHT : DIALOG_TEXT_NORMAL;
            overlay_draw_rect(x + 1, y, x + width - 2, y + GLYPH_HEIGHT + 3, border_color);

            // Draw background
            overlay_draw_rectfill(x + 2, y + 1, x + width - 3, y + GLYPH_HEIGHT + 2, DIALOG_BG_NORMAL);

            // Draw text
            if (control->data.inputbox.buffer) {
                overlay_draw_simple_text(control->data.inputbox.buffer, x + 3, y + 3, DIALOG_TEXT_NORMAL);

                // Draw cursor if in input mode
                if (focused && (dialog->cursor_blink & 8)) {
                    int cursor_x = x + 3 + overlay_get_text_width(control->data.inputbox.buffer);
                    overlay_draw_vline(cursor_x, y + 3, y + GLYPH_HEIGHT + 1, DIALOG_TEXT_NORMAL);
                }
            }
            break;
        }

        case DIALOG_SPACING: {
            // Just empty space - no rendering needed
            break;
        }

        case DIALOG_BUTTONBAR: {
            bool quick_mode = is_quick_dialog_mode(dialog);
            const char *left_text = NULL;
            const char *right_text = NULL;
            bool show_shortcuts = quick_mode;

            switch (control->data.buttonbar.type) {
                case BUTTONBAR_OK_ONLY:
                    left_text = show_shortcuts ? "\x8e ok" : "ok";
                    break;
                case BUTTONBAR_OK_CANCEL:
                    left_text = show_shortcuts ? "\x8e ok" : "ok";
                    right_text = show_shortcuts ? "\x97 cancel" : "cancel";
                    break;
                case BUTTONBAR_YES_NO:
                    left_text = show_shortcuts ? "\x8e yes" : "yes";
                    right_text = show_shortcuts ? "\x97 no" : "no";
                    break;
                case BUTTONBAR_CANCEL_ONLY:
                    left_text = show_shortcuts ? "\x97 cancel" : "cancel";
                    break;
            }

            if (!quick_mode && focused) {
                // Highlight the focused button
                if (dialog->focused_subcontrol == 0 && left_text) {
                    int text_width = overlay_get_text_width(left_text);
                    overlay_draw_rectfill(x, y, x + text_width + 1, y + GLYPH_HEIGHT + 1, DIALOG_BG_HIGHLIGHT);
                } else if (dialog->focused_subcontrol == 1 && right_text) {
                    int left_width = left_text ? overlay_get_text_width(left_text) + 4 : 0;
                    int text_width = overlay_get_text_width(right_text);
                    overlay_draw_rectfill(x + left_width, y, x + left_width + text_width + 1,
                                        y + GLYPH_HEIGHT + 1, DIALOG_BG_HIGHLIGHT);
                }
            }

            int text_x = x + 1;
            if (left_text) {
                int text_color = (!quick_mode && focused && dialog->focused_subcontrol == 0) ?
                                DIALOG_TEXT_HIGHLIGHT : DIALOG_TEXT_NORMAL;
                overlay_draw_simple_text(left_text, text_x, y + 2, text_color);
                text_x += overlay_get_text_width(left_text) + 4;
            }

            if (right_text) {
                int text_color = (!quick_mode && focused && dialog->focused_subcontrol == 1) ?
                                DIALOG_TEXT_HIGHLIGHT : DIALOG_TEXT_NORMAL;
                overlay_draw_simple_text(right_text, text_x, y + 2, text_color);
            }
            break;
        }

        case DIALOG_LISTBOX: {
            int list_height = get_control_height(control);

            // Draw border and background
            if (control->data.listbox.draw_border) {
                overlay_draw_rect(x + 1, y, x + width - 2, y + list_height - 1, DIALOG_TEXT_NORMAL);
                overlay_draw_rectfill(x + 2, y + 1, x + width - 4, y + list_height - 2, DIALOG_BG_NORMAL);
            } else {
                overlay_draw_rectfill(x, y, x + width - 1, y + list_height - 1, DIALOG_BG_NORMAL);
            }

            // Draw items
            int selected_idx = control->data.listbox.selected_index ? *control->data.listbox.selected_index : 0;
            int scroll_offset = control->data.listbox.scroll_offset;
            int item_offset_x = control->data.listbox.draw_border ? 2 : 0;
            int item_offset_y = control->data.listbox.draw_border ? 1 : 0;
            int content_height = list_height - item_offset_y * 2;

            // Adjust scroll to keep selected item visible
            if (selected_idx * (GLYPH_HEIGHT + 1) < scroll_offset)
                scroll_offset = selected_idx * (GLYPH_HEIGHT + 1);
            else if ((selected_idx + 1) * (GLYPH_HEIGHT + 1) >= scroll_offset + list_height - item_offset_y - 1)
                scroll_offset = (selected_idx + 1) * (GLYPH_HEIGHT + 1) - content_height;

            // Clamp scroll
            if (scroll_offset > (control->data.listbox.item_count * (GLYPH_HEIGHT + 1)) - content_height)
                scroll_offset = (control->data.listbox.item_count * (GLYPH_HEIGHT + 1)) - content_height;
            if (scroll_offset < 0)
                scroll_offset = 0;

            // Update scroll offset (casting away const - this is internal state)
            ((p8_dialog_control_t *)control)->data.listbox.scroll_offset = scroll_offset;

            int text_offset = control->data.listbox.draw_border ? 3 : 1;
            int first_visible_line = scroll_offset / (GLYPH_HEIGHT + 1);
            int last_visible_lne = (scroll_offset + list_height + GLYPH_HEIGHT - 2) / (GLYPH_HEIGHT + 1);

            overlay_clip_set(x + item_offset_x, y + item_offset_y, width - item_offset_x * 2, list_height - item_offset_y * 2);

            for (int i = first_visible_line; i < last_visible_lne && i < control->data.listbox.item_count; i++) {
                int item_y = y + item_offset_y + 1 + i * (GLYPH_HEIGHT + 1) - scroll_offset;
                bool is_selected = (i == selected_idx);

                // Determine colors based on selection and focus state
                int item_bg_color, item_fg_color;
                if (is_selected) {
                    if (focused) {
                        // Focused listbox with selected item: yellow background
                        item_bg_color = DIALOG_BG_HIGHLIGHT;
                        item_fg_color = DIALOG_TEXT_HIGHLIGHT;
                    } else {
                        // Unfocused listbox with selected item: white background (inverted label style)
                        item_bg_color = DIALOG_BG_INVERTED;
                        item_fg_color = DIALOG_TEXT_INVERTED;
                    }
                } else {
                    // Unselected item: normal colors
                    item_bg_color = DIALOG_BG_NORMAL;
                    item_fg_color = DIALOG_TEXT_NORMAL;
                }

                if (control->data.listbox.render_callback) {
                    // Use custom rendering callback
                    int item_x = x + item_offset_x;
                    int item_w = width - item_offset_x * 2;
                    control->data.listbox.render_callback(
                        dialog,
                        control->data.listbox.user_data,
                        i,
                        is_selected,
                        item_x,
                        item_y,
                        item_w,
                        GLYPH_HEIGHT,
                        item_fg_color,
                        item_bg_color
                    );
                } else {
                    // Default rendering for string items
                    if (is_selected) {
                        int hl_x0 = x + item_offset_x;
                        int hl_x1 = x + width - item_offset_x - 1;
                        overlay_draw_rectfill(hl_x0, item_y - 1, hl_x1, item_y + GLYPH_HEIGHT - 1, item_bg_color);
                    }

                    const char *item_text = control->data.listbox.items[i];
                    overlay_draw_simple_text(item_text, x + text_offset, item_y, item_fg_color);
                }
            }

            overlay_clip_reset();
            break;
        }

        case DIALOG_CUSTOM: {
            if (control->data.custom.draw_fn)
                control->data.custom.draw_fn(dialog, control->data.custom.user_data, x, y, width, get_control_height(control));
            break;
        }
    }
}

/* Draw the dialog */
void p8_dialog_draw(const p8_dialog_t *dialog)
{
    int x = dialog->x;
    int y = dialog->y;
    int width = dialog->width;
    int height = dialog->height;

    // Draw border if enabled
    if (dialog->draw_border) {
        overlay_draw_rect(x, y, x + width - 1, y + height - 1, DIALOG_BORDER_OUTER);
        overlay_draw_rect(x + 1, y + 1, x + width - 2, y + height - 2, DIALOG_BORDER_INNER);
    }

    // Draw background
    int bg_x0 = dialog->draw_border ? x + 2 : x;
    int bg_y0 = dialog->draw_border ? y + 2 : y;
    int bg_x1 = dialog->draw_border ? x + width - 3 : x + width - 1;
    int bg_y1 = dialog->draw_border ? y + height - 3 : y + height - 1;
    overlay_draw_rectfill(bg_x0, bg_y0, bg_x1, bg_y1, DIALOG_BG_NORMAL);

    int content_y = dialog->draw_border ? y + CONTROL_PADDING_Y : y;
    int content_x = dialog->draw_border ? x + CONTROL_PADDING_X : x;
    int content_width = dialog->draw_border ? width - CONTROL_PADDING_X * 2 : width;

    // Draw title if present
    if (dialog->title) {
        overlay_draw_simple_text(dialog->title, content_x + 1, content_y, DIALOG_TEXT_NORMAL);
        content_y += GLYPH_HEIGHT;

        // Draw separator line
        overlay_draw_hline(content_x + 1, content_x + content_width - 2, content_y + 1, DIALOG_TEXT_NORMAL);
        content_y += SEPARATOR_HEIGHT + CONTROL_PADDING_Y;
    }

    // Draw controls
    for (int i = 0; i < dialog->control_count; i++) {
        bool focused = (i == dialog->focused_control);
        draw_control(dialog, i, content_x, content_y, content_width, focused);

        content_y += get_control_height(&dialog->controls[i]);
        if (i < dialog->control_count - 1 && dialog->padding > 0) {
            content_y += dialog->padding;
        }
    }
}

/* Draw all currently showing dialogs in the stack (for refreshing underlying dialogs). */
void p8_dialog_draw_stack(void)
{
    for (int i = 0; i < m_dialog_nest_count; i++)
        p8_dialog_draw(m_dialog_stack[i]);
}

/* Clear the dialog overlay region */
void p8_dialog_clear(p8_dialog_t *dialog)
{
    int x = dialog->x;
    int y = dialog->y;
    int width = dialog->width;
    int height = dialog->height;

    overlay_draw_rectfill(x, y, x + width - 1, y + height - 1, 0);
}

/* Helper function to move to next selectable control */
static bool move_to_next_control(p8_dialog_t *dialog, bool quick_mode)
{
    int next = dialog->focused_control + 1;
    while (next < dialog->control_count &&
        (!dialog->controls[next].selectable ||
            (quick_mode && dialog->controls[next].type == DIALOG_BUTTONBAR))) {
        next++;
    }

    if (next < dialog->control_count) {
        dialog->focused_control = next;
        dialog->focused_subcontrol = 0;
    }

    return next < dialog->control_count;
}

/* Helper function to move to previous selectable control */
static bool move_to_prev_control(p8_dialog_t *dialog, bool quick_mode)
{
    int prev = dialog->focused_control - 1;
    while (prev >= 0 &&
        (!dialog->controls[prev].selectable ||
            (quick_mode && dialog->controls[prev].type == DIALOG_BUTTONBAR))) {
        prev--;
    }

    if (prev >= 0) {
        dialog->focused_control = prev;
        dialog->focused_subcontrol = 0;
    }

    return prev >= 0;
}

/* Process dialog input and update state */
p8_dialog_action_t p8_dialog_update(p8_dialog_t *dialog)
{
    p8_dialog_action_t result = { DIALOG_RESULT_NONE, 0 };

    uint16_t buttons = m_buttonsp[0];

    bool quick_mode = is_quick_dialog_mode(dialog);

    // Update cursor blink
    dialog->cursor_blink++;

    if (dialog->focused_control >= 0) {
        // Handle input mode (for input boxes)
        if (dialog->controls[dialog->focused_control].type == DIALOG_INPUTBOX) {
            p8_dialog_control_t *control = &dialog->controls[dialog->focused_control];

            uint8_t keypress;
            while (p8_get_next_keypress(NULL, &keypress, NULL)) {
                // Handle Return
                if (keypress == 13) {
                    if (!move_to_next_control(dialog, quick_mode)) {
                        // No more controls, accept the dialog
                        result.type = DIALOG_RESULT_ACCEPTED;
                    }

                    return result;
                }
                // Handle printable characters
                else if (keypress >= 32 && keypress <= 126) {
                    int len = strlen(control->data.inputbox.buffer);
                    if (len < control->data.inputbox.buffer_size - 1) {
                        control->data.inputbox.buffer[len] = keypress;
                        control->data.inputbox.buffer[len + 1] = '\0';
                    }
                    return result;
                }
                // Handle backspace
                else if (keypress == 8) {
                    int len = strlen(control->data.inputbox.buffer);
                    if (len > 0) {
                        control->data.inputbox.buffer[len - 1] = '\0';
                    }
                    return result;
                }
            }
        } else {
            // Action buttons
            p8_dialog_control_t *control = &dialog->controls[dialog->focused_control];

            // Button 1 (O) / Space activates focused control
            if (buttons & (BUTTON_MASK_ACTION1 | BUTTON_MASK_SPACE | BUTTON_MASK_RETURN)) {
                switch (control->type) {
                    case DIALOG_BUTTON:
                    case DIALOG_MENUITEM:
                        result.type = DIALOG_RESULT_BUTTON;
                        result.action_id = control->data.button.action_id;
                        break;

                    case DIALOG_CHECKBOX:
                        if (control->data.checkbox.value) {
                            *control->data.checkbox.value = !*control->data.checkbox.value;
                        }
                        break;

                    case DIALOG_LISTBOX:
                        if (!move_to_next_control(dialog, quick_mode)) {
                            // No more controls, accept the dialog
                            result.type = DIALOG_RESULT_ACCEPTED;
                        }
                        break;

                    case DIALOG_BUTTONBAR:
                        if (!quick_mode) {
                            // Activate the focused button
                            p8_dialog_control_t *buttonbar = &dialog->controls[dialog->focused_control];
                            if (buttonbar->data.buttonbar.type == BUTTONBAR_CANCEL_ONLY) {
                                result.type = DIALOG_RESULT_CANCELLED;
                            } else if (dialog->focused_subcontrol == 0) {
                                result.type = DIALOG_RESULT_ACCEPTED;
                            } else {
                                result.type = DIALOG_RESULT_CANCELLED;
                            }
                        }
                        break;

                    default:
                        break;
                }
            }
        }
    }

    // Handle Return / (Button 1 / Space in quick mode) (before checking focused control)
    if ((quick_mode && (buttons & (BUTTON_MASK_RETURN | BUTTON_MASK_ACTION1 | BUTTON_MASK_SPACE)))) {
        result.type = DIALOG_RESULT_ACCEPTED;
        return result;
    }

    // Escape / (Button 2 (X) in quick mode) cancels dialog
    if ((buttons & BUTTON_MASK_ESCAPE) || (quick_mode && (buttons & BUTTON_MASK_ACTION2))) {
        result.type = DIALOG_RESULT_CANCELLED;
        return result;
    }

    // Navigation - Up/Down for controls
    if (buttons & BUTTON_MASK_UP) {
        if (dialog->focused_control >= 0) {
            // Special handling for listbox
            p8_dialog_control_t *control = &dialog->controls[dialog->focused_control];
            if (control->type == DIALOG_LISTBOX && control->data.listbox.selected_index) {
                int *sel = control->data.listbox.selected_index;
                if (*sel > 0) {
                    (*sel)--;
                }
                return result;  // Stay in listbox, don't change focus
            }

            // Move to previous selectable control
            move_to_prev_control(dialog, quick_mode);
        }
    }

    if (buttons & BUTTON_MASK_DOWN) {
        if (dialog->focused_control >= 0) {
            // Special handling for listbox
            p8_dialog_control_t *control = &dialog->controls[dialog->focused_control];
            if (control->type == DIALOG_LISTBOX && control->data.listbox.selected_index) {
                int *sel = control->data.listbox.selected_index;
                if (*sel < control->data.listbox.item_count - 1) {
                    (*sel)++;
                }
                return result;  // Stay in listbox, don't change focus
            }

            // Move to next selectable control
            move_to_next_control(dialog, quick_mode);
        }
    }

    if (buttons & (BUTTON_MASK_PAGE_UP | BUTTON_MASK_PAGE_DOWN)) {
        if (dialog->focused_control >= 0) {
            p8_dialog_control_t *control = &dialog->controls[dialog->focused_control];
            if (control->type == DIALOG_LISTBOX && control->data.listbox.selected_index) {
                int *sel = control->data.listbox.selected_index;
                // Compute page size as the number of fully-visible items based
                // on the actual pixel height of the listbox.
                int item_offset_y = control->data.listbox.draw_border ? 1 : 0;
                int content_height = get_control_height(control) - item_offset_y * 2;
                int page_size = content_height / (GLYPH_HEIGHT + 1);
                if (page_size < 1) page_size = 1;
                if (buttons & BUTTON_MASK_PAGE_UP) {
                    *sel -= page_size;
                    if (*sel < 0) *sel = 0;
                } else {
                    *sel += page_size;
                    if (*sel >= control->data.listbox.item_count)
                        *sel = control->data.listbox.item_count - 1;
                }
                return result;
            }
        }
    }

    // Left/Right for button bar sub-controls and listbox navigation
    if (dialog->focused_control >= 0) {
        p8_dialog_control_t *control = &dialog->controls[dialog->focused_control];
        if (control->type == DIALOG_BUTTONBAR) {
            int max_subcontrol = (control->data.buttonbar.type == BUTTONBAR_OK_ONLY ||
                                  control->data.buttonbar.type == BUTTONBAR_CANCEL_ONLY) ? 0 : 1;

            if (buttons & BUTTON_MASK_LEFT) {
                if (dialog->focused_subcontrol > 0) {
                    dialog->focused_subcontrol--;
                    return result;
                }
            }
            if (buttons & BUTTON_MASK_RIGHT) {
                if (dialog->focused_subcontrol < max_subcontrol) {
                    dialog->focused_subcontrol++;
                    return result;
                }
            }
        }
    }

    // MENUITEM controls capture Left/Right and return them as a button event
    // rather than navigating focus, so the cart callback receives them.
    if (buttons & (BUTTON_MASK_LEFT | BUTTON_MASK_RIGHT)) {
        if (dialog->focused_control >= 0) {
            p8_dialog_control_t *ctrl = &dialog->controls[dialog->focused_control];
            if (ctrl->type == DIALOG_MENUITEM) {
                result.type = DIALOG_RESULT_BUTTON;
                result.action_id = ctrl->data.button.action_id;
                result.button_mask = buttons & (BUTTON_MASK_LEFT | BUTTON_MASK_RIGHT);
                return result;
            }
        }
    }

    // LEFT/RIGHT navigate to previous/next control
    if (buttons & BUTTON_MASK_LEFT) {
        move_to_prev_control(dialog, quick_mode);
    }
    if (buttons & BUTTON_MASK_RIGHT) {
        move_to_next_control(dialog, quick_mode);
    }

    return result;
}

/* Run modal dialog loop */
p8_dialog_action_t p8_dialog_run(p8_dialog_t *dialog)
{
    p8_dialog_action_t result = { DIALOG_RESULT_NONE, 0 };

    p8_dialog_set_showing(dialog, true);

    // Main dialog loop
    do {
        p8_dialog_draw(dialog);
        p8_flip();  // p8_flip calls p8_update_input internally

        result = p8_dialog_update(dialog);
    } while (result.type == DIALOG_RESULT_NONE && !p8_is_quit_requested());

    p8_dialog_set_showing(dialog, false);

    while (p8_get_next_keypress(NULL, NULL, NULL)) {
        // Clear any remaining keypresses to avoid them affecting the next dialog
    }

    return result;
}

/* Clean up dialog resources */
void p8_dialog_cleanup(p8_dialog_t *dialog)
{
    if (m_dialog_nest_count > 0 && m_dialog_stack[m_dialog_nest_count - 1] == dialog)
        p8_dialog_set_showing(dialog, false);
}

/* Set the dialog showing state */
void p8_dialog_set_showing(p8_dialog_t *dialog, bool showing)
{
    (void)dialog;
    if (!showing) {
        if (m_dialog_nest_count > 0) {
            assert(m_dialog_stack[m_dialog_nest_count - 1] == dialog);
            m_dialog_nest_count--;
            m_dialog_stack[m_dialog_nest_count] = NULL;
        }
        // Draw the dialogs underneath the closed one to refresh the screen
        p8_dialog_clear(dialog);
        p8_dialog_draw_stack();
        p8_flip();
    } else if (showing) {
        assert(m_dialog_nest_count < MAX_DIALOG_NESTING);
        m_dialog_stack[m_dialog_nest_count] = dialog;
        m_dialog_nest_count++;
    }
    m_dialog_showing = m_dialog_nest_count > 0;
}
