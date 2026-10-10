/**
 * Copyright (C) 2025 Chris January
 */

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#if defined(__APPLE__)
  #include <stdlib.h>
  #include <malloc/malloc.h>
#elif defined(__OpenBSD__)
  #include <stdlib.h>
#else
  #include <malloc.h>
#endif
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "p8_browse.h"
#include "p8_dialog.h"
#include "p8_emu.h"
#include "p8_lua_helper.h"
#include "p8_options.h"
#include "p8_overlay_helper.h"
#include "p8_pause_menu.h"
#include "strtcpy.h"

#define FALLBACK_CARTS_PATH "."

#define INITIAL_FILENAME_MEM_SIZE 8192
#define INITIAL_CAPACITY 100

static char *filename_mem = NULL;
static char *filename_mem_ptr = NULL;
static char *filename_mem_end = NULL;

struct dir_entry {
    char *file_name;
    bool is_dir;
};

static struct dir_entry *dir_contents = NULL;
static int nitems = 0;
static int capacity = 0;
static int current_item = 0;
static void clear_dir_contents(void) {
    filename_mem_ptr = filename_mem;
    nitems = 0;
    current_item = 0;
}
static void append_dir_entry(const char *file_name, bool is_dir)
{
   if (nitems >= capacity) {
        int new_capacity;
        if (capacity == 0)
            new_capacity = 10;
        else
            new_capacity = capacity * 2;
        struct dir_entry *new_contents = realloc(dir_contents, sizeof(dir_contents[0]) * new_capacity);
        if (!new_contents) {
            fputs("Out of memory\n", stderr);
            return;
        }
        dir_contents = new_contents;
        capacity = new_capacity;
    }
    struct dir_entry *dir_entry = &dir_contents[nitems];
    size_t name_len = strlen(file_name);
    if (filename_mem_ptr + name_len + 1 > filename_mem_end) {
        size_t old_size = filename_mem_end - filename_mem;
        size_t new_size = old_size * 2;
        size_t offset = filename_mem_ptr - filename_mem;
        char *new_filename_mem = realloc(filename_mem, new_size);
        if (!new_filename_mem) {
            fputs("Out of memory\n", stderr);
            return;
        }
        filename_mem_ptr = new_filename_mem + offset;
        filename_mem_end = new_filename_mem + new_size;
        filename_mem = new_filename_mem;
    }
    dir_entry->file_name = filename_mem_ptr;
    memcpy(dir_entry->file_name, file_name, name_len + 1);
    filename_mem_ptr += name_len + 1;
    dir_entry->is_dir = is_dir;
    nitems++;
}

static int compare_dir_entry(const void *p1, const void *p2)
{
    struct dir_entry *dir_entry1 = (struct dir_entry *)p1;
    struct dir_entry *dir_entry2 = (struct dir_entry *)p2;
    if (dir_entry1->is_dir != dir_entry2->is_dir)
        return (dir_entry1->is_dir ? 0 : 1) - (dir_entry2->is_dir ? 0 : 1);
    else
        return strcmp(dir_entry1->file_name, dir_entry2->file_name);
}
static void list_dir(const char* path) {
    if (strtcpy(m_current_cart_dir, path, sizeof(m_current_cart_dir)) < 0) {
        fputs("Path too long\n", stderr);
        return;
    }
    p8_show_io_icon(true);
    DIR *dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
    } else {
        clear_dir_contents();
        char full_path[PATH_MAX];
        for (;;) {
            struct dirent *dirent;
            errno = 0;
            dirent = readdir(dir);
            if (dirent == NULL) {
                if (errno != 0)
                    fprintf(stderr, "%s: %s\n", path, strerror(errno));
                break;
            }
            if (strcmp(dirent->d_name, ".") == 0)
                continue;
            if (p8_make_full_path(full_path, sizeof(full_path), m_current_cart_dir, dirent->d_name) != 0) {
                fputs("Path too long\n", stderr);
                continue;
            }
            bool is_dir = false;
            if (full_path[0] == '\0' ||
                (full_path[1] == ':' &&
                 (full_path[2] == '/' || full_path[2] == '\\') &&
                 full_path[3] == '\0')) {
                is_dir = true;
            } else {
                struct stat statbuf;
                int res = stat(full_path, &statbuf);
                if (res == 0)
                    is_dir = S_ISDIR(statbuf.st_mode);
                else
                    fprintf(stderr, "%s: %s\n", full_path, strerror(errno));
            }
            append_dir_entry(dirent->d_name, is_dir);
        }
        closedir(dir);
    }
    qsort(dir_contents, nitems, sizeof(dir_contents[0]), compare_dir_entry);
    strtcpy(m_current_cart_dir, path, sizeof(m_current_cart_dir));
    p8_show_io_icon(false);
}

void draw_file_name(const char *str, int x, int y, int col)
{
    int cursor_x = x;
    for (const char *c = str; *c != '\0'; c++) {
        char display_char;
        if (*c >= 'a' && *c <= 'z') {
             display_char = *c - ('a' - 'A');
        } else if (*c >= 'A' && *c <= 'Z') {
            display_char = *c + ('a' - 'A');
        } else if (*c >= 0x20 && *c <= 0x7F) {
            display_char = *c;
        } else {
            display_char = '?';
        }
        overlay_draw_char(display_char, cursor_x, y, col);
        cursor_x += GLYPH_WIDTH;
    }
}

static void render_file_item(const p8_dialog_t *dialog, void *user_data, int index, bool selected, int x, int y, int width, int height, int fg_color, int bg_color)
{
    (void)dialog;
    (void)user_data;
    struct dir_entry *dir_entry = &dir_contents[index];

    if (selected)
        overlay_draw_rectfill(x, y - 1, x + width - 1, y + height - 1, bg_color);

    // // Clip to avoid drawing filename over " <dir>" suffix
    int clip_x, clip_y, clip_w, clip_h;
    overlay_clip_get(&clip_x, &clip_y, &clip_w, &clip_h);
    if (dir_entry->is_dir)
        overlay_clip_set(x, y, width - GLYPH_WIDTH * 6, height);
    else
        overlay_clip_set(x, y, width, height);

    // Draw filename with case conversion
    draw_file_name(dir_entry->file_name, x, y, fg_color);

    overlay_clip_set(clip_x, clip_y, clip_w, clip_h);

    // Draw " <dir>" suffix for directories
    if (dir_entry->is_dir)
        overlay_draw_simple_text(" <dir>", x + width - GLYPH_WIDTH * 6, y, fg_color);
}

static int show_menu(void)
{
    p8_dialog_control_t controls[] = {
        DIALOG_MENUITEM("show version", 1),
        DIALOG_MENUITEM("controls", 2),
        DIALOG_MENUITEM("back", 3)
    };

    p8_dialog_t dialog;
    p8_dialog_init(&dialog, NULL, controls, sizeof(controls) / sizeof(controls[0]), 0);
    p8_dialog_action_t result = p8_dialog_run(&dialog);
    p8_dialog_cleanup(&dialog);

    if (result.type == DIALOG_RESULT_BUTTON)
        return result.action_id;
    else
        return 0;
}

int browse_for_cart(char *cart_path, size_t cart_path_size)
{
    filename_mem = (char *)malloc(INITIAL_FILENAME_MEM_SIZE);
    dir_contents = (struct dir_entry *)malloc(sizeof(struct dir_entry) * INITIAL_CAPACITY);
    if (!filename_mem || !dir_contents) {
        free(filename_mem);
        free(dir_contents);
        fputs("Out of memory\n", stderr);
        return -1;
    }
    capacity = INITIAL_CAPACITY;
    filename_mem_end = filename_mem + INITIAL_FILENAME_MEM_SIZE;
    clear_dir_contents();

    p8_reset();

    if (access(DEFAULT_CARTS_PATH, F_OK) == 0)
        list_dir(DEFAULT_CARTS_PATH);
    else
        list_dir(FALLBACK_CARTS_PATH);

    int selected_index = 0;

    // Create dialog with custom listbox renderer
    p8_dialog_control_t controls[] = {
        DIALOG_LABEL_INVERTED(""),
        DIALOG_LISTBOX_CUSTOM_FULLSCREEN(NULL, nitems, &selected_index, render_file_item, NULL),
        DIALOG_LABEL_INVERTED("\216: select file  \227: menu"),
    };

    p8_dialog_t dialog;
    p8_dialog_init(&dialog, NULL, controls, sizeof(controls) / sizeof(controls[0]), P8_WIDTH);
    dialog.draw_border = false;
    dialog.padding = 0;

    p8_dialog_action_t result = { DIALOG_RESULT_NONE, 0 };
    p8_dialog_set_showing(&dialog, true);

    while (!p8_is_quit_requested()) {
        selected_index = 0;
        controls[0].label = m_current_cart_dir;
        controls[1].data.listbox.item_count = nitems;

        // Main dialog loop
        do {
            p8_dialog_draw(&dialog);
            p8_flip();

            result = p8_dialog_update(&dialog);

            if (result.type == DIALOG_RESULT_NONE && ((m_buttonsp[0] & BUTTON_MASK_ACTION2) != 0)) {
                int action_id = show_menu();
                switch (action_id) {
                    case 1:
                        p8_show_version_dialog();
                        break;
                    case 2:
                        p8_show_controls_dialog();
                        break;
                    default:
                        break;
                }
            }
        } while (result.type == DIALOG_RESULT_NONE && !p8_is_quit_requested());

        if (cart_path[0])
            break;

        if (result.type == DIALOG_RESULT_CANCELLED)
            break;

        if (result.type == DIALOG_RESULT_ACCEPTED && selected_index >= 0 && selected_index < nitems) {
            struct dir_entry *dir_entry = &dir_contents[selected_index];
            char full_path[PATH_MAX] = {'\0'};
            if (p8_make_full_path(full_path, sizeof(full_path), m_current_cart_dir, dir_entry->file_name) < 0) {
                fputs("Path too long\n", stderr);
                continue;
            }
            if (dir_entry->is_dir) {
                list_dir(full_path);
                selected_index = 0;
            } else {
                if (strtcpy(cart_path, full_path, cart_path_size) < 0) {
                    fputs("Path too long\n", stderr);
                    continue;
                }
                break;
            }
        }
    }

    p8_dialog_set_showing(&dialog, false);
    p8_dialog_cleanup(&dialog);

    overlay_clear();
    p8_flip();

    clear_dir_contents();

    free(filename_mem);
    free(dir_contents);
    filename_mem = NULL;
    dir_contents = NULL;

    return 0;
}
