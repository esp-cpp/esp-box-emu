#include <common.h>
#include <libgte.h>
#include <libgpu.h>

const char* D_800B8860[] = {
    "F3", "FT3", "F4", "FT4", "G3", "GT3", "G4",  "GT4",
    "LF", "LF",  "LF", "LF",  "GF", "GF",  "GF",  "GF",
    "T",  "S",   "T1", "S1",  "T8", "S8",  "T16", "S16"};
const char D_800B3000[] = "BF";
const char type_unk[] = "?";

void DrawOTagIO(OT_TYPE* p) {
    while (!isendprim(p)) {
        if (getlen(p)) {
            DrawPrim(p);
        }
        p = nextPrim(p);
    }
    if (getlen(p)) {
        DrawPrim(p);
    }
}

void DumpOTag(OT_TYPE* p) {
    const char* type;
    const char* default_name;
    int n_prim;
    u_char code;

    n_prim = 1;
    GPU_printf("OTag :prim\n%5d:", NULL);
    while (!isendprim(p)) {
        if (!getlen(p)) {
            if (getlen(nextPrim(p))) {
                GPU_printf("\n%5d:", n_prim);
            }
            n_prim++;
        } else {
            CheckPrim("DumpOTage", p);
            code = getcode(p);
            if (code == 2) {
                type = D_800B3000;
            } else if (code >= 0x20 && code < 0x80) {
                type = D_800B8860[(code - 0x20) / 4];
            } else {
                type = type_unk;
            }
            GPU_printf("-%s", type);
        }
        p = nextPrim(p);
    }
    CheckPrim("DumpOTage", p);
    code = getcode(p);
    default_name = type_unk;
    if (code == 2) {
        type = D_800B3000;
    } else if (code >= 0x20 && code < 0x80) {
        type = D_800B8860[(code - 0x20) / 4];
    } else {
        type = default_name;
    }
    GPU_printf("-%s\n", type);
}

u_char prim_len[] = {
    0x04, 0x07, 0x05, 0x09, 0x06, 0x09, 0x08, 0x0C, 0x03, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x03, 0x04, 0x02, 0x03, 0x02, 0x03, 0x02, 0x03};
int CheckPrim(char* s, OT_TYPE* p) {
    const char* type;
    u_char len;
    u_char code;

    code = getcode(p);
    if (code == 2) {
        len = 3;
    } else if (code >= 0x20 && code < 0x80) {
        len = prim_len[(code - 0x20) / 4];
    } else {
        len = 0;
    }
    if (len > 0 && len != getlen(p)) {
        code = getcode(p);
        if (code == 2) {
            type = D_800B3000;
        } else if (code >= 0x20 && code < 0x80) {
            type = D_800B8860[(code - 0x20) / 4];
        } else {
            type = type_unk;
        }
        GPU_printf(
            "%s: bad prim:addr=%08X,type=%s,len=%d\n", s, p, type, getlen(p));
        return -1;
    }
    len = prim_len[(code - 0x20) / 4]; // FAKE
    return 0;
}