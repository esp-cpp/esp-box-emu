/*
 * p8_parser.c
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "lodepng.h"
#include "p8_emu.h"
#include "pico8.h"
#include "p8_lua_helper.h"
#include "strtcpy.h"

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#define RAW_DATA_LENGTH 0x4300
#define IMAGE_WIDTH 160
#define IMAGE_HEIGHT 205

enum P8Type
{
    P8TYPE_START = 0,
    P8TYPE_IDENT,
    P8TYPE_VERSION,
    P8TYPE_LUA,
    P8TYPE_GFX_4BIT,
    P8TYPE_GFX_8BIT,
    P8TYPE_GFF,
    P8TYPE_LABEL,
    P8TYPE_MAP,
    P8TYPE_SFX,
    P8TYPE_MUSIC,
    P8TYPE_COUNT,
};

static const char *m_p8_name[] = {
    NULL,
    "pico-8 cartridge",
    "version",
    "__lua__",
    "__gfx__",
    "__gfx8__",
    "__gff__",
    "__label__",
    "__map__",
    "__sfx__",
    "__music__",
};

static int m_p8_mem_offset[] = {
    0,
    0,
    0,
    0,
    MEMORY_SPRITES,
    0,
    MEMORY_SPRITEFLAGS,
    0,
    MEMORY_MAP,
    MEMORY_SFX,
    MEMORY_MUSIC,
};

#if defined(_WIN32) || defined(__MINGW32__)
static char* strsep_portable(char **stringp, const char *delim)
{
    char *start, *p;
    if (stringp == NULL || *stringp == NULL) return NULL;

    start = *stringp;
    for (p = start; *p; ++p) {
        const char *d;
        for (d = delim; *d; ++d) {
            if (*p == *d) {
                *p = '\0';
                *stringp = p + 1;
                return start;
            }
        }
    }
    *stringp = NULL;
    return start;
}
#define strsep strsep_portable
#endif

int parse_cart_ram(uint8_t *buffer, int size, uint8_t *memory, uint8_t *decompression_buffer, char *lua_script_out, uint8_t *label_image);
int parse_cart_file(const char *file_name, uint8_t *memory, uint8_t *file_buffer, uint8_t *decompression_buffer, char *lua_script_out, uint8_t *label_image);
int parse_png_ram(const char *file_name, uint8_t *buffer, int file_size, uint8_t *memory, uint8_t *decompression_buffer, const char **lua_script, uint8_t *label_image);
static int parse_p8_ram(const char *file_name, uint8_t *buffer, int size, uint8_t *memory, uint8_t *decompression_buffer, const char **lua_script, uint8_t *label_image);
static int process_includes(char *lua_script_out, const char *lua_script, const char *cart_dir);
static void convert_utf8_to_p8scii(uint8_t *buffer, size_t len);

static uint8_t PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};

static int parse_cart_ram0(const char *file_name, uint8_t *buffer, int size, uint8_t *memory, uint8_t *decompression_buffer, const char **lua_script, uint8_t *label_image)
{
    if (size >= 8 &&
        memcmp(buffer, PNG_SIGNATURE, 8) == 0) {
        return parse_png_ram(file_name, buffer, size, memory, decompression_buffer, lua_script, label_image);
    } else {
        return parse_p8_ram(file_name, buffer, size, memory, decompression_buffer, lua_script, label_image);
    }
}

int parse_cart_ram(uint8_t *buffer, int size, uint8_t *memory, uint8_t *decompression_buffer, char *lua_script_out, uint8_t *label_image)
{
    const char *lua_script = NULL;
    if (parse_cart_ram0(NULL, buffer, size, memory, decompression_buffer, &lua_script, label_image) != 0)
        return -1;

    if (lua_script) {
        size_t lua_len = strnlen(lua_script, LUA_SCRIPT_SIZE);
        memcpy(lua_script_out, lua_script, lua_len);
        lua_script_out[lua_len] = '\0';
    } else {
        lua_script_out[0] = '\0';
    }

    return 0;
}

// process_includes: expand #include directives from lua_script into lua_script_out.
// Returns true if any includes were processed and lua_script_out was populated.
// Returns false if no includes found (caller should copy lua_script into lua_script_out).
static int process_includes(char *lua_script_out, const char *lua_script, const char *cart_dir)
{
    // Quick scan: any #include lines present?
    const char *scan = lua_script;
    bool has_includes = false;
    while ((scan = strstr(scan, "#include")) != NULL) {
        // Must be at start of line (or start of string)
        if (scan == lua_script || scan[-1] == '\n') {
            has_includes = true;
            break;
        }
        scan += 8;
    }
    if (!has_includes) {
        strtcpy(lua_script_out, lua_script, LUA_SCRIPT_SIZE);
        return 0;
    }

    // Build expanded content into lua_script_out
    size_t capacity = LUA_SCRIPT_SIZE - 1;
    size_t length = 0;
    char *result = lua_script_out;

    const char *p = lua_script;
    while (*p) {
        // Find end of current line
        const char *eol = strchr(p, '\n');
        int line_len = eol ? (int)(eol - p) : (int)strlen(p);

        // Check for #include at start of line
        if (strncmp(p, "#include ", 9) == 0) {
            // Extract filename (skip "#include ")
            const char *fname_start = p + 9;
            int fname_len = line_len - 9;
            // Trim trailing whitespace/CR
            while (fname_len > 0 && (fname_start[fname_len - 1] == ' ' ||
                                     fname_start[fname_len - 1] == '\r'))
                fname_len--;

            if (fname_len > 0) {
                // Build full path relative to the cart file's directory
                char include_path[PATH_MAX];
                ssize_t dir_len_p = strtcpy(include_path, cart_dir, sizeof(include_path));
                size_t avail = (dir_len_p >= 0) ? sizeof(include_path) - (size_t)dir_len_p : 0;
                if (avail >= 2 && (size_t)fname_len < avail - 1) {
                    include_path[dir_len_p] = '/';
                    memcpy(include_path + dir_len_p + 1, fname_start, fname_len);
                    include_path[dir_len_p + 1 + fname_len] = '\0';
                } else {
                    fprintf(stderr, "Warning: #include path too long\n");
                    errno = ENAMETOOLONG;
                    return -1;
                }

                FILE *inc = fopen(include_path, "rb");
                if (inc) {
                    fseek(inc, 0, SEEK_END);
                    long inc_size = ftell(inc);
                    rewind(inc);

                    if (length + (size_t)inc_size >= capacity) {
                        fprintf(stderr, "Warning: #include would exceed lua script buffer\n");
                        fclose(inc);
                        errno = E2BIG;
                        return -1;
                    } else {
                        if (fread(result + length, 1, inc_size, inc) != (size_t)inc_size) {
                            int orig_errno = errno;
                            fprintf(stderr, "Error reading include file: %s\n", include_path);
                            fclose(inc);
                            errno = orig_errno;
                            return -1;
                        }
                        convert_utf8_to_p8scii((uint8_t *)(result + length), inc_size);
                        length += strlen(result + length);
                        fclose(inc);

                        // Ensure included content ends with newline
                        if (length > 0 && result[length - 1] != '\n' && length < capacity)
                            result[length++] = '\n';
                    }
                } else {
                    int orig_errno = errno;
                    fprintf(stderr, "Warning: #include file not found: %s\n", include_path);
                    errno = orig_errno;
                    return -1;
                }
            }
        } else {
            // Copy line as-is (including newline)
            int copy_len = eol ? line_len + 1 : line_len;
            if (length + (size_t)copy_len >= capacity) {
                fprintf(stderr, "Warning: lua script truncated at buffer limit\n");
                errno = E2BIG;
                return -1;
            }
            memcpy(result + length, p, copy_len);
            length += copy_len;
        }

        p += line_len;
        if (eol)
            p++; // skip newline
    }

    result[length] = '\0';
    return length;
}

int parse_cart_file(const char *file_name, uint8_t *memory, uint8_t *file_buffer, uint8_t *decompression_buffer, char *lua_script_out, uint8_t *label_image)
{
    FILE *file = fopen(file_name, "rb");

    if (file == NULL)
    {
        fprintf(stderr, "Error opening file: %s\n", file_name);
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    if ((size_t)file_size >= FILE_BUFFER_SIZE) {
        fprintf(stderr, "Error: cart file too large (%ld bytes): %s\n", file_size, file_name);
        fclose(file);
        return -1;
    }

    if (fread(m_file_buffer, 1, file_size, file) != (size_t)file_size) {
        fprintf(stderr, "Error reading file: %s\n", file_name);
        fclose(file);
        return -1;
    }

    fclose(file);

    m_file_buffer[file_size] = '\0';

    const char *lua_script = NULL;
    if (parse_cart_ram0(file_name, m_file_buffer, (int)file_size, memory, decompression_buffer, &lua_script, label_image) != 0)
        return -1;

    // lua_script either points into file_buffer or decompression_buffer

    if (lua_script_out) {
        // Process #include directives in the Lua section
        if (lua_script && file_name) {
            const char *last_slash = strrchr(file_name, '/');
            char cart_dir[PATH_MAX];
            if (last_slash) {
                size_t dir_len = last_slash - file_name;
                if (dir_len >= sizeof(cart_dir))
                    dir_len = sizeof(cart_dir) - 1;
                memcpy(cart_dir, file_name, dir_len);
                cart_dir[dir_len] = '\0';
            } else {
                strtcpy(cart_dir, ".", sizeof(cart_dir));
            }

            // process_includes writes into lua_script_out directly
            int ret = process_includes(lua_script_out, lua_script, cart_dir);
            if (ret < 0)
                return ret;
        } else if (lua_script) {
            size_t lua_len = strnlen(lua_script, LUA_SCRIPT_SIZE);
            memcpy(lua_script_out, lua_script, lua_len);
            lua_script_out[lua_len] = '\0';
        } else {
            lua_script_out[0] = '\0';
        }
    }

    return 0;
}

static void convert_utf8_to_p8scii(uint8_t *buffer, size_t len)
{
    uint8_t *read_ptr = buffer;
    uint8_t *write_ptr = buffer;
    uint8_t *end_ptr = buffer + len;

    while (read_ptr < end_ptr) {
        if (*read_ptr >= 0x80) {
            uint8_t symbol_length;
            int p8_index = get_p8_symbol((const char *)read_ptr, end_ptr - read_ptr, &symbol_length);
            
            if (p8_index >= 0) {
                *write_ptr++ = (uint8_t)p8_index;
                read_ptr += symbol_length;
            } else {
                *write_ptr++ = *read_ptr++;
            }
        } else {
            *write_ptr++ = *read_ptr++;
        }
    }

    *write_ptr = '\0';
}

void hex_to_bytes(uint8_t *memory, char *str, int str_len, int *write_length)
{
    *write_length = 0;

    if (str == NULL)
        return;

    if (str_len == 0)
        return;

    int read_offset = 0;
    int write_offset = 0;

    while (read_offset < str_len)
    {
        if (!isxdigit(str[read_offset]))
        {
            read_offset++;
            continue;
        }
        unsigned int v;
        sscanf(str + read_offset, "%2x", &v);
        memory[write_offset] = (uint8_t)v;
        read_offset += 2;
        write_offset++;
    }

    *write_length = write_offset;
}

void read_sfx(uint8_t *dest, uint8_t *src, int read_length, int *write_length)
{
    int read_offset = 0;
    int write_offset = 0;

    while (read_offset < read_length)
    {
        uint8_t byte_0 = src[read_offset++];
        uint8_t byte_1 = src[read_offset++];
        uint8_t byte_2 = src[read_offset++];
        uint8_t byte_3 = src[read_offset++];

        for (int i = 0; i < 16; i++)
        {
            uint8_t byte_a = src[read_offset++];
            uint8_t byte_b = src[read_offset++];
            uint8_t byte_c = src[read_offset++];
            uint8_t byte_d = src[read_offset++];
            uint8_t byte_e = src[read_offset++];

            uint8_t note1_pitch = byte_a;
            uint8_t note1_waveform = byte_b >> 4;
            uint8_t note1_volume = byte_b & 0xF;
            uint8_t note1_effect = byte_c >> 4;

            uint8_t note2_pitch = (byte_c << 4) | (byte_d >> 4);
            uint8_t note2_waveform = byte_d & 0xF;
            uint8_t note2_volume = byte_e >> 4;
            uint8_t note2_effect = byte_e & 0xF;

            uint16_t note1 = (note1_waveform > 7 ? 1 << 15 : 0) |
                             (note1_effect << 12) | (note1_volume << 9) |
                             ((note1_waveform > 7 ? note1_waveform - 8 : note1_waveform) << 6) |
                             note1_pitch;
            uint16_t note2 = (note2_waveform > 7 ? 1 << 15 : 0) |
                             (note2_effect << 12) | (note2_volume << 9) |
                             ((note2_waveform > 7 ? note2_waveform - 8 : note2_waveform) << 6) |
                             note2_pitch;

            dest[write_offset++] = note1;
            dest[write_offset++] = note1 >> 8;
            dest[write_offset++] = note2;
            dest[write_offset++] = note2 >> 8;
        }

        dest[write_offset++] = byte_0; // Editor Mode
        dest[write_offset++] = byte_1; // Speed
        dest[write_offset++] = byte_2; // Loop Start
        dest[write_offset++] = byte_3; // Loop End
    }

    *write_length = write_offset;
}

void read_music(uint8_t *dest, uint8_t *src, int read_length, int *write_length)
{
    int read_offset = 0;
    int write_offset = 0;

    while (read_offset < read_length)
    {
        uint8_t flags_byte = src[read_offset++];
        uint8_t effect_id1 = src[read_offset++];
        uint8_t effect_id2 = src[read_offset++];
        uint8_t effect_id3 = src[read_offset++];
        uint8_t effect_id4 = src[read_offset++];

        // On-disk flags bits 0-3 map to bit 7 of in-memory bytes 0-3
        dest[write_offset++] = (effect_id1 & 0x7F) | ((flags_byte & (1 << 0)) ? 0x80 : 0);
        dest[write_offset++] = (effect_id2 & 0x7F) | ((flags_byte & (1 << 1)) ? 0x80 : 0);
        dest[write_offset++] = (effect_id3 & 0x7F) | ((flags_byte & (1 << 2)) ? 0x80 : 0);
        dest[write_offset++] = (effect_id4 & 0x7F) | ((flags_byte & (1 << 3)) ? 0x80 : 0);
    }

    *write_length = write_offset;
}

static int parse_p8_ram(const char *file_name, uint8_t *buffer, int size, uint8_t *memory, uint8_t *decompression_buffer, const char **lua_script, uint8_t *label_image)
{
    static uint8_t tmpbuf[180];
    char *line;
    char *rest = (char *)buffer;
    int lua_start = 0, lua_end = 0;
    int p8_type = P8TYPE_START;
    int file_offset = 0;
    int write_offset = 0;
    int read_length = 0;
    int write_length = 0;
    bool seen_ident = false;

    if (lua_script)
        *lua_script = NULL;

    if (label_image)
        memset(label_image, 0, 0x4000);

    while ((line = strsep(&rest, "\n")) != NULL)
    {
        int token_found = 0;
        int line_length = rest ? (rest - line) : (size - file_offset);

        if (file_offset > 0)
            line[-1] = '\n';

        file_offset += line_length;

        for (int i = P8TYPE_IDENT; i < P8TYPE_COUNT; i++)
        {
            if (strncmp(line, m_p8_name[i], strlen(m_p8_name[i])) == 0)
            {
                p8_type = i;
                write_offset = 0;
                token_found = 1;
                break;
            }
        }

        if (token_found)
        {
            if (p8_type == P8TYPE_LUA)
            {
                lua_start = file_offset;
                lua_end = file_offset;
            }
            else if (p8_type == P8TYPE_IDENT)
            {
                seen_ident = true;
            }
            continue;
        }

        switch (p8_type)
        {
        case P8TYPE_LUA:
        {
            lua_end += line_length;
            break;
        }
        case P8TYPE_GFX_4BIT:
        // case P8TYPE_GFX_8BIT:
        case P8TYPE_GFF:
        case P8TYPE_MAP:
        {
            uint8_t *write_mem = memory + m_p8_mem_offset[p8_type] + write_offset;
            hex_to_bytes(write_mem, line, line_length-1, &write_length);

            write_offset += write_length;
            break;
        }
        case P8TYPE_LABEL:
        {
            if (label_image) {
                const char *hex_chars = "0123456789abcdefghijklmnopqrstuv";
                int x = 0;
                for (int i = 0; i < line_length - 1 && x < 128; i++) {
                    char c = line[i];
                    const char *pos = strchr(hex_chars, c);
                    if (pos) {
                        label_image[write_offset * 128 + x] = (uint8_t)(pos - hex_chars);
                        x++;
                    }
                }
                if (x > 0) write_offset++;
            }
            break;
        }
        case P8TYPE_SFX:
        {
            uint8_t *write_mem = memory + MEMORY_SFX + write_offset;

            hex_to_bytes(tmpbuf, line, line_length-1, &read_length);

            if (read_length > 0)
                read_sfx(write_mem, tmpbuf, read_length, &write_length);

            write_offset += write_length;
            break;
        }
        case P8TYPE_MUSIC:
        {
            uint8_t *write_mem = memory + MEMORY_MUSIC + write_offset;

            hex_to_bytes(tmpbuf, line, line_length-1, &read_length);

            if (read_length > 0)
                read_music(write_mem, tmpbuf, read_length, &write_length);

            write_offset += write_length;
            break;
        }
        }
    }

    if (!seen_ident) {
        fprintf(stderr, "Error: %s: missing pico-8 cartridge header\n", file_name ? file_name : "<buffer>");
        return 1;
    }

    for (int i = MEMORY_SPRITES; i < MEMORY_SPRITES + MEMORY_SPRITES_SIZE; i++)
        memory[i] = NIBBLE_SWAP(memory[i]);

    for (int i = MEMORY_SPRITES_MAP; i < MEMORY_SPRITES_MAP + MEMORY_SPRITES_MAP_SIZE; i++)
        memory[i] = NIBBLE_SWAP(memory[i]);

    buffer[lua_end] = '\0';

    convert_utf8_to_p8scii(&buffer[lua_start], lua_end - lua_start);

    if (lua_script && lua_start)
        *lua_script = (const char *) &buffer[lua_start];

    return 0;
}

#define PNG_WIDTH 160
#define PNG_HEIGHT 205

int parse_png_ram(const char *file_name, uint8_t *buffer, int file_size, uint8_t *memory, uint8_t *decompression_buffer, const char **lua_script, uint8_t *label_image)
{
    if (lua_script)
        *lua_script = NULL;
    uint8_t *px_buffer = NULL;
    unsigned width = 0, height = 0;
    unsigned ret = lodepng_decode32(&px_buffer, &width, &height, buffer, file_size);
    if (ret != 0) {
        fprintf(stderr, "%s\n", lodepng_error_text(ret));
        return -1;
    }
    if (width != PNG_WIDTH || height != PNG_HEIGHT) {
        free(px_buffer);
        if (file_name)
            fprintf(stderr, "%s: ", file_name);
        fprintf(stderr, "PNG has wrong size: %dx%d (expected 160x205)\n", width, height);
        return -1;
    }

    if (label_image) {
        // 32-colour extended palette (R, G, B) with the two least significant bits masked off
        static const uint8_t palette[32][3] = {
            {0, 0, 0}, {28, 40, 80}, {124, 36, 80}, {0, 132, 80},
            {168, 80, 52}, {92, 84, 76}, {192, 192, 196}, {252, 240, 232},
            {252, 0, 76}, {252, 160, 0}, {252, 236, 36}, {0, 228, 52},
            {40, 172, 252}, {128, 116, 156}, {252, 116, 168}, {252, 204, 168},
            {40, 24, 20}, {16, 28, 52}, {64, 32, 52}, {16, 80, 88},
            {116, 44, 40}, {72, 48, 56}, {160, 136, 120}, {240, 236, 124},
            {188, 16, 80}, {252, 108, 36}, {168, 228, 44}, {0, 180, 64},
            {4, 88, 180}, {116, 68, 100}, {252, 108, 88}, {252, 156, 128}
        };

        const int label_left = 16;
        const int label_top = 24;

        for (int y = 0; y < 128; y++) {
            for (int x = 0; x < 128; x += 2) {
                int left_offset = ((label_top + y) * PNG_WIDTH + (label_left + x)) << 2;
                int right_offset = ((label_top + y) * PNG_WIDTH + (label_left + x + 1)) << 2;

                uint8_t left_r = px_buffer[left_offset] & 0xFC;
                uint8_t left_g = px_buffer[left_offset + 1] & 0xFC;
                uint8_t left_b = px_buffer[left_offset + 2] & 0xFC;

                uint8_t right_r = px_buffer[right_offset] & 0xFC;
                uint8_t right_g = px_buffer[right_offset + 1] & 0xFC;
                uint8_t right_b = px_buffer[right_offset + 2] & 0xFC;

                // Find matching palette colour index for each pixel
                uint8_t left_color = 0;
                for (int i = 0; i < 32; i++) {
                    if (palette[i][0] == left_r &&
                        palette[i][1] == left_g &&
                        palette[i][2] == left_b) {
                        left_color = i;
                        break;
                    }
                }

                uint8_t right_color = 0;
                for (int i = 0; i < 32; i++) {
                    if (palette[i][0] == right_r &&
                        palette[i][1] == right_g &&
                        palette[i][2] == right_b) {
                        right_color = i;
                        break;
                    }
                }

                label_image[y * 128 + x] = left_color;
                label_image[y * 128 + x + 1] = right_color;
            }
        }
    }

    uint8_t *px_buffer_in = px_buffer;
    uint8_t *byte_buffer = px_buffer;
    uint8_t *byte_buffer_out = byte_buffer;
    for (unsigned i=0;i<PNG_WIDTH*PNG_HEIGHT;++i) {
        *byte_buffer_out = ((px_buffer_in[3] & 0x3) << 6) | ((px_buffer_in[0] & 0x3) << 4) | ((px_buffer_in[1] & 0x3) << 2) | (px_buffer_in[2] & 0x3);
        px_buffer_in += 4;
        byte_buffer_out++;
    }
    memcpy(memory, byte_buffer, CART_MEMORY_SIZE);
    {
        pico8_code_section_decompress(byte_buffer + CART_MEMORY_SIZE, decompression_buffer, LUA_SCRIPT_SIZE - 1);
        if (lua_script)
            *lua_script = (const char *)decompression_buffer;
    }

    free(px_buffer);

    return 0;
}
