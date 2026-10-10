/**
 * Copyright (C) 2026 Chris January
 *
 * Dialog API for femto8
 */

#ifndef P8_DIALOG_H
#define P8_DIALOG_H

#include <stdbool.h>
#include <stdint.h>

/* Forward declaration for dialog type used in callbacks */
typedef struct p8_dialog p8_dialog_t;

/* Dialog Control Types */
typedef enum {
    DIALOG_LABEL,      // Read-only text label
    DIALOG_BUTTON,     // Selectable button (highlighted when focused)
    DIALOG_MENUITEM,   // Menu item button (full width)
    DIALOG_CHECKBOX,   // Toggle checkbox with label
    DIALOG_INPUTBOX,   // Single-line text input field
    DIALOG_SPACING,    // Visual separator (non-interactive)
    DIALOG_BUTTONBAR,  // Horizontal Ok/Cancel button bar (conditionally selectable, acts on O/X)
    DIALOG_LISTBOX,    // Scrollable list box with selection (selectable)
    DIALOG_CUSTOM      // Custom-drawn region with user-supplied draw function
} p8_dialog_control_type_t;

/* Button Bar Types */
typedef enum {
    BUTTONBAR_OK_ONLY,     // Single Ok button
    BUTTONBAR_OK_CANCEL,   // Ok and Cancel buttons
    BUTTONBAR_YES_NO,      // Yes and No buttons
    BUTTONBAR_CANCEL_ONLY  // Single Cancel button
} p8_dialog_buttonbar_type_t;

/* Dialog Control Structure */
typedef struct {
    p8_dialog_control_type_t type;
    const char *label;           // Display text (all control types)

    // Type-specific data (union)
    union {
        struct {
            bool *value;         // Pointer to checkbox state (CHECKBOX)
        } checkbox;

        struct {
            char *buffer;        // Text buffer (INPUTBOX)
            int buffer_size;     // Maximum buffer size
            int cursor_pos;      // Current cursor position
        } inputbox;

        struct {
            int action_id;       // Button action identifier (BUTTON)
        } button;

        struct {
            p8_dialog_buttonbar_type_t type;  // Button bar variant (BUTTONBAR)
        } buttonbar;

        struct {
            int width;    // Minimum content width required (0 = no constraint)
            int height;   // Control height in pixels
            void (*draw_fn)(const p8_dialog_t *dialog, void *user_data, int x, int y, int width, int height); // Draw function
            void *user_data; // User data pointer passed to draw function
        } custom;

        struct {
            union {
                const char **items;  // Array of item strings (standard listbox)
                void *user_data;     // Custom user data (when using render callback)
            };
            int item_count;      // Number of items in list
            int *selected_index; // Pointer to selected item index
            int visible_lines;   // Number of visible lines (0 = auto)
            int scroll_offset;   // Current scroll position (internal)
            bool draw_border;    // Whether to draw border around listbox
            void (*render_callback)(const p8_dialog_t *dialog, void *user_data, int index, bool selected, int x, int y, int width, int height, int fg_color, int bg_color);
        } listbox;
    } data;

    bool selectable;             // Whether control can receive focus
    bool enabled;                // Whether control is active
    bool inverted;               // Use inverted colors (for headers/footers)
} p8_dialog_control_t;

/* Dialog Structure */
struct p8_dialog {
    const char *title;                  // Dialog title (optional, can be NULL)
    p8_dialog_control_t *controls;      // Array of controls
    int control_count;                  // Number of controls

    int focused_control;                // Currently focused control index
    int focused_subcontrol;             // Currently focused sub-control (for button bar)
    int width;                          // Dialog width in pixels
    int height;                         // Dialog height in pixels (auto-calculated)
    int padding;                        // Pixels of spacing between controls (default: 2)
    bool draw_border;                   // Whether to draw double border (default: true)

    // Position (auto-centered if set to -1)
    int x;
    int y;

    // Internal state
    int cursor_blink;                   // Cursor blink counter for input boxes
    bool input_mode;                    // True when actively editing an inputbox
};

/* Dialog Result */
typedef enum {
    DIALOG_RESULT_NONE,         // No action yet
    DIALOG_RESULT_ACCEPTED,     // Dialog accepted (Button 1 / Return)
    DIALOG_RESULT_CANCELLED,    // Dialog cancelled (Button 2 / Escape)
    DIALOG_RESULT_BUTTON        // Specific button pressed (check action_id)
} p8_dialog_result_t;

typedef struct {
    p8_dialog_result_t type;
    int action_id;              // For DIALOG_RESULT_BUTTON
    uint16_t button_mask;       // For DIALOG_RESULT_BUTTON on MENUITEM: button mask of triggering button(s)
} p8_dialog_action_t;

/* Color Scheme */
#define DIALOG_BORDER_OUTER       1   // Dark blue (outer border)
#define DIALOG_BORDER_INNER       7   // White (inner border)
#define DIALOG_BG_NORMAL          1   // Dark blue (dialog background)
#define DIALOG_BG_HIGHLIGHT       10  // Yellow (focused/selected control)
#define DIALOG_TEXT_NORMAL        7   // White (normal text)
#define DIALOG_TEXT_HIGHLIGHT     1   // Dark blue (text on highlighted background)
#define DIALOG_BG_INVERTED        7   // White (inverted background)
#define DIALOG_TEXT_INVERTED      1   // Dark blue (inverted text)

/* Global Dialog State */
extern bool m_dialog_showing;

/* Control Initialization Macros */

/**
 * Create a label control (non-selectable text).
 */
#define DIALOG_LABEL(text) \
    { .type = DIALOG_LABEL, .label = (text), \
      .selectable = false, .enabled = true, \
      .inverted = false }

/**
 * Create an inverted label control (white-on-dark-blue for headers/footers).
 */
#define DIALOG_LABEL_INVERTED(text) \
    { .type = DIALOG_LABEL, .label = (text), \
      .selectable = false, .enabled = true, \
      .inverted = true }

/**
 * Create a button control.
 */
#define DIALOG_BUTTON(text, id) \
    { .type = DIALOG_BUTTON, .label = (text), \
      .data.button.action_id = (id), \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a menu item control (full width, no vertical padding).
 */
#define DIALOG_MENUITEM(text, id) \
    { .type = DIALOG_MENUITEM, .label = (text), \
      .data.button.action_id = (id), \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a checkbox control.
 */
#define DIALOG_CHECKBOX(text, val_ptr) \
    { .type = DIALOG_CHECKBOX, .label = (text), \
      .data.checkbox.value = (val_ptr), \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create an input box control.
 */
#define DIALOG_INPUTBOX(text, buf, size) \
    { .type = DIALOG_INPUTBOX, .label = (text), \
      .data.inputbox = { .buffer = (buf), \
                         .buffer_size = (size), \
                         .cursor_pos = 0 }, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a visual separator (non-selectable).
 */
#define DIALOG_SPACING() \
    { .type = DIALOG_SPACING, .label = NULL, \
      .selectable = false, .enabled = true, \
      .inverted = false }

/**
 * Create a horizontal button bar with Ok/Cancel buttons.
 * Displays as: "ok  cancel"
 * Uses fixed action IDs: DIALOG_ACTION_OK and DIALOG_ACTION_CANCEL.
 */
#define DIALOG_BUTTONBAR() \
    { .type = DIALOG_BUTTONBAR, .label = NULL, \
      .data.buttonbar.type = BUTTONBAR_OK_CANCEL, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a button bar with only an Ok button (for message boxes).
 * Use this for simple message dialogs where cancel doesn't make sense.
 */
#define DIALOG_BUTTONBAR_OK_ONLY() \
    { .type = DIALOG_BUTTONBAR, .label = NULL, \
      .data.buttonbar.type = BUTTONBAR_OK_ONLY, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a horizontal button bar with Yes/No buttons.
 * Uses fixed action IDs: DIALOG_ACTION_YES and DIALOG_ACTION_NO.
 */
#define DIALOG_BUTTONBAR_YES_NO() \
    { .type = DIALOG_BUTTONBAR, .label = NULL, \
      .data.buttonbar.type = BUTTONBAR_YES_NO, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a button bar with only a Cancel button.
 * Use this for waiting dialogs where the user can only cancel.
 */
#define DIALOG_BUTTONBAR_CANCEL_ONLY() \
    { .type = DIALOG_BUTTONBAR, .label = NULL, \
      .data.buttonbar.type = BUTTONBAR_CANCEL_ONLY, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a scrollable list box.
 * Use Up/Down to navigate, O/Space to select (quick dialog mode only).
 * visible_lines: number of lines to display (0 for auto-sizing based on item_count)
 */
#define DIALOG_LISTBOX(title, item_array, count, sel_idx_ptr, lines) \
    { .type = DIALOG_LISTBOX, .label = (title), \
      .data.listbox = { .items = (item_array), \
                        .item_count = (count), \
                        .selected_index = (sel_idx_ptr), \
                        .visible_lines = (lines), \
                        .scroll_offset = 0, \
                        .draw_border = true, \
                        .render_callback = NULL }, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a custom-rendered list box with a rendering callback.
 * Use this when you need complete control over item rendering (e.g., file browsers with metadata).
 * The callback receives: user_data, index, x, y, width, height, fg_color, bg_color
 */
#define DIALOG_LISTBOX_CUSTOM(title, usr_data, count, sel_idx_ptr, lines, callback) \
    { .type = DIALOG_LISTBOX, .label = (title), \
      .data.listbox = { .user_data = (usr_data), \
                        .item_count = (count), \
                        .selected_index = (sel_idx_ptr), \
                        .visible_lines = (lines), \
                        .scroll_offset = 0, \
                        .draw_border = true, \
                        .render_callback = (callback) }, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a scrollable list box without a border.
 * Use this variant for full-screen file browsers or when integrating with custom borders.
 */
#define DIALOG_LISTBOX_FULLSCREEN(title, item_array, count, sel_idx_ptr) \
    { .type = DIALOG_LISTBOX, .label = (title), \
      .data.listbox = { .items = (item_array), \
                        .item_count = (count), \
                        .selected_index = (sel_idx_ptr), \
                        .visible_lines = 0, \
                        .scroll_offset = 0, \
                        .draw_border = false, \
                        .render_callback = NULL }, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/**
 * Create a custom-rendered list box with a rendering callback.
 * Use this when you need complete control over item rendering (e.g., file browsers with metadata).
 * The callback receives: user_data, index, x, y, width, height, fg_color, bg_color
 */
/**
 * Create a custom-drawn control with a user-supplied draw function.
 * The draw function receives the top-left coordinates, available width, and
 * the control height as declared here.
 * @param w    Minimum content width needed (0 = no constraint; use for auto-sizing).
 * @param h    Control height in pixels.
 * @param fn   Draw callback: void fn(int x, int y, int width, int height)
 * @param user_data  User data pointer passed to draw function (can be NULL)
 */
#define DIALOG_CUSTOM_CONTROL(w, h, fn, usr_data) \
    { .type = DIALOG_CUSTOM, .label = NULL, \
      .data.custom = { .width = (w), .height = (h), .draw_fn = (fn), .user_data = (usr_data) }, \
      .selectable = false, .enabled = true, \
      .inverted = false }

/**
 * Create a custom-rendered list box with a rendering callback.
 * Use this when you need complete control over item rendering (e.g., file browsers with metadata).
 * The callback receives: user_data, index, x, y, width, height, fg_color, bg_color
 */
#define DIALOG_LISTBOX_CUSTOM_FULLSCREEN(title, count, sel_idx_ptr, callback, usr_data) \
    { .type = DIALOG_LISTBOX, .label = (title), \
      .data.listbox = { .user_data = (usr_data), \
                        .item_count = (count), \
                        .selected_index = (sel_idx_ptr), \
                        .visible_lines = 0, \
                        .scroll_offset = 0, \
                        .draw_border = false, \
                        .render_callback = (callback) }, \
      .selectable = true, .enabled = true, \
      .inverted = false }

/* Core API Functions */

/**
 * Initialize a dialog structure with default values.
 * Sets draw_border=true and padding=2 by default.
 *
 * @param dialog     Pointer to dialog structure to initialize
 * @param title      Dialog title (can be NULL)
 * @param controls   Array of controls to display
 * @param count      Number of controls in array
 * @param width      Dialog width (or 0 for auto-sizing)
 */
void p8_dialog_init(p8_dialog_t *dialog,
                    const char *title,
                    p8_dialog_control_t *controls,
                    int count,
                    int width);

/**
 * Draw the dialog to the overlay buffer.
 * This function is stateless and can be called multiple times.
 *
 * @param dialog     Pointer to dialog to render
 */
void p8_dialog_draw(const p8_dialog_t *dialog);

/**
 * Draw all currently showing dialogs in the stack (for refreshing underlying dialogs).
 */
void p8_dialog_draw_stack(void);

/**
 * Process input for the dialog and update its state.
 * Returns action indicating what happened (if anything).
 * Uses m_buttonsp and m_keypress global variables.
 *
 * @param dialog     Pointer to dialog to update
 * @return           Action result
 */
p8_dialog_action_t p8_dialog_update(p8_dialog_t *dialog);

/**
 * Run a modal dialog loop until accepted or cancelled.
 * Handles input, drawing, and flipping automatically.
 *
 * @param dialog     Pointer to dialog to run
 * @return           Final dialog action
 */
p8_dialog_action_t p8_dialog_run(p8_dialog_t *dialog);

/**
 * Set the dialog showing state.
 *
 * @param dialog     Pointer to dialog (unused)
 * @param showing    True if a dialog is being shown, false otherwise
 */
void p8_dialog_set_showing(p8_dialog_t *dialog, bool showing);

/**
 * Clean up dialog resources (if any were allocated).
 */
void p8_dialog_cleanup(p8_dialog_t *dialog);

#endif /* P8_DIALOG_H */
