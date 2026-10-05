#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "p8_emu.h"
#include "p8_cstore.h"
#include "p8_symbols.h"

static void write_sfx(FILE *f, const uint8_t *src)
{
    for (int entry = 0; entry < 64; entry++) {
        const uint8_t *e = src + entry * 68;
        uint8_t editor_mode = e[64];
        uint8_t speed       = e[65];
        uint8_t loop_start  = e[66];
        uint8_t loop_end    = e[67];
        fprintf(f, "%02x%02x%02x%02x", editor_mode, speed, loop_start, loop_end);
        for (int pair = 0; pair < 16; pair++) {
            uint16_t note1 = (uint16_t)e[pair*4]   | ((uint16_t)e[pair*4+1] << 8);
            uint16_t note2 = (uint16_t)e[pair*4+2] | ((uint16_t)e[pair*4+3] << 8);
            uint8_t n1_pitch    = note1 & 0x3F;
            uint8_t n1_waveform = ((note1 >> 6) & 0x7) + ((note1 & 0x8000) ? 8 : 0);
            uint8_t n1_volume   = (note1 >> 9) & 0x7;
            uint8_t n1_effect   = (note1 >> 12) & 0x7;
            uint8_t n2_pitch    = note2 & 0x3F;
            uint8_t n2_waveform = ((note2 >> 6) & 0x7) + ((note2 & 0x8000) ? 8 : 0);
            uint8_t n2_volume   = (note2 >> 9) & 0x7;
            uint8_t n2_effect   = (note2 >> 12) & 0x7;
            uint8_t byte_a = n1_pitch;
            uint8_t byte_b = (n1_waveform << 4) | n1_volume;
            uint8_t byte_c = (n1_effect   << 4) | (n2_pitch >> 4);
            uint8_t byte_d = ((n2_pitch & 0xF) << 4) | n2_waveform;
            uint8_t byte_e = (n2_volume << 4) | n2_effect;
            fprintf(f, "%02x%02x%02x%02x%02x", byte_a, byte_b, byte_c, byte_d, byte_e);
        }
        fprintf(f, "\n");
    }
}

static void write_music(FILE *f, const uint8_t *src)
{
    for (int i = 0; i < 64; i++) {
        uint8_t b0 = src[i*4];
        uint8_t b1 = src[i*4+1];
        uint8_t b2 = src[i*4+2];
        uint8_t b3 = src[i*4+3];
        uint8_t flags = ((b0 >> 7) & 1) | (((b1 >> 7) & 1) << 1)
                      | (((b2 >> 7) & 1) << 2) | (((b3 >> 7) & 1) << 3);
        fprintf(f, "%02x %02x%02x%02x%02x\n",
                flags, b0 & 0x7F, b1 & 0x7F, b2 & 0x7F, b3 & 0x7F);
    }
}

int write_cart_p8(const char *path, const char *lua_script, const uint8_t *memory)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "pico-8 cartridge // http://www.pico-8.com\nversion 43\n__lua__\n");
    if (lua_script && *lua_script) {
        const char *p = lua_script;
        for (; *p; p++) {
            uint8_t c = *p;
            if (c < 32 || c >= 127) {
                bool found = false;
                for (int i=0; i<sizeof(p8_symbols)/sizeof(p8_symbol_t); i++) {
                    if (c == p8_symbols[i].index) {
                        fwrite(p8_symbols[i].encoding, 1, p8_symbols[i].length, f);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    fputc(*p, f);
            } else {
                fputc(*p, f);
            }
        }
        if (p[-1] != '\n')
            fputc('\n', f);
    } else {
        fprintf(f, "\n");
    }

    // GFX: 0x0000-0x1FFF, 128 lines x 64 bytes, each byte NIBBLE_SWAP'd before output
    fprintf(f, "__gfx__\n");
    for (int row = 0; row < 128; row++) {
        for (int col = 0; col < 64; col++)
            fprintf(f, "%02x", (uint8_t)NIBBLE_SWAP(memory[row * 64 + col]));
        fprintf(f, "\n");
    }

    // GFF: 0x3000-0x30FF, 2 lines x 128 bytes
    fprintf(f, "__gff__\n");
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 128; col++)
            fprintf(f, "%02x", memory[MEMORY_SPRITEFLAGS + row * 128 + col]);
        fprintf(f, "\n");
    }

    // MAP: 0x2000-0x2FFF, 32 lines x 128 bytes
    fprintf(f, "__map__\n");
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 128; col++)
            fprintf(f, "%02x", memory[MEMORY_MAP + row * 128 + col]);
        fprintf(f, "\n");
    }

    // SFX: 64 entries x 68 bytes
    fprintf(f, "__sfx__\n");
    write_sfx(f, memory + MEMORY_SFX);

    // Music: 64 patterns x 4 bytes
    fprintf(f, "__music__\n");
    write_music(f, memory + MEMORY_MUSIC);

    fprintf(f, "\n");
    fclose(f);
    return 0;
}
