#include <psyz.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define PSYZ_ESP_XRAM EXT_RAM_BSS_ATTR
#else
#define PSYZ_ESP_XRAM
#endif
#include <libgpu.h>
#include <psyz/log.h>
#include "../draw.h"
#include "../internal.h"

static u32 GPU_STATUS = 0;

typedef enum {
    // https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gpu-versions
    GPU_V0, // 1MB VRAM, retail
    GPU_V1, // 2MB VRAM, arcade
    GPU_V2, // 2MB VRAM, PSone
} GpuVersion;

struct Gpu {
    /* 0x00 */ const char* ver;
    /* 0x04 */ int (*addque)(
        int (*exec)(u_long p1, u_long p2), u_long p1, u_long p2);
    /* 0x08 */ int (*addque2)(
        int (*exec)(u_long p1, u_long p2), u_long p1, int len, u_long p2);
    /* 0x0C */ int (*clr)(RECT* rect, u32 color);
    /* 0x10 */ void (*ctl)(unsigned int);
    /* 0x14 */ int (*cwb)();
    /* 0x18 */ int (*cwc)(u_long p1, u_long p2);
    /* 0x1C */ int (*drs)(u_long p1, u_long p2);
    /* 0x20 */ int (*dws)(u_long p1, u_long p2);
    /* 0x24 */ int (*exeque)();
    /* 0x28 */ int (*getctl)(int);
    /* 0x2C */ void (*otc)(OT_TYPE* ot, s32 n);
    /* 0x30 */ int (*param)(int);
    /* 0x34 */ int (*reset)(int);
    /* 0x38 */ u_long (*status)(void);
    /* 0x3C */ int (*sync)(int mode);
};

u32 get_vram_wh(void);

static void GPU_clear_cache() { NOT_IMPLEMENTED; }

static void GPU_write_image() { NOT_IMPLEMENTED; }

static void GPU_read_image() { NOT_IMPLEMENTED; }

#ifdef ESP_PLATFORM
// queue_len gets overwritten by a still-unlocated wild DRAM writer on the
// NZ0 boot path; these canaries give a hardware watchpoint a quiet target
// (nothing legitimate ever writes them)
int psyz_queue_canary_lo;
static int queue_len = 0;
int psyz_queue_canary_hi;
#else
static int queue_len = 0;
#endif
PSYZ_ESP_XRAM static u_long queue_buf[0x4000];
#ifdef ESP_PLATFORM
/* rough phase timing: where does the frame actually go? */
unsigned psyz_prof_exeq_us, psyz_prof_load_us, psyz_prof_load_n;
unsigned psyz_prim_by_code[8];
static long long psyz_prof_t0;
#endif
#ifdef ESP_PLATFORM
extern unsigned sotn_move_count, sotn_exeque_count;
#endif

int Psyz_GpuExeque() {
    RECT rect;
    unsigned int x, y;
#ifdef ESP_PLATFORM
    sotn_exeque_count++;
    { extern long long esp_timer_get_time(void);
      psyz_prof_t0 = esp_timer_get_time(); }
#endif
    Draw_ResetBuffer();
    for (int i = 0; i < queue_len; i++) {
        u_long op = queue_buf[i];
        int code = (int)(queue_buf[i] >> 24) & 0xFF;
        // https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gpu-render-polygon-commands
        switch (code) {
        case 0x00:
            // empty?!
            break;
        case 0x01: // clear cache
            GPU_clear_cache();
            break;
        case 0x02: // frame buffer rectangle draw
            rect.x = (short)(queue_buf[i + 1] & 0xFFFF);
            rect.y = (short)((queue_buf[i + 1] >> 16) & 0xFFFF);
            rect.w = (short)(queue_buf[i + 2] & 0xFFFF);
            rect.h = (short)((queue_buf[i + 2] >> 16) & 0xFFFF);
            Draw_ClearImage(
                &rect, (u_char)(op & 0xFF), (u_char)((op >> 8) & 0xFF),
                (u_char)((op >> 16) & 0xFF));
            i += 2;
            break;
        case 0x80: // move image; the GPU truncates coords to 10/9 bits
            rect.x = (short)(queue_buf[i + 1] & 0x3FF);
            rect.y = (short)((queue_buf[i + 1] >> 16) & 0x1FF);
            rect.w = (short)(queue_buf[i + 3] & 0x7FF);
            rect.h = (short)((queue_buf[i + 3] >> 16) & 0x3FF);
            x = queue_buf[i + 2] & 0x3FF;
            y = (queue_buf[i + 2] >> 16) & 0x1FF;
            Draw_MoveImage(&rect, x, y);
#ifdef ESP_PLATFORM
            sotn_move_count++;
#endif
            i += 3; // consume the three parameter words or the parser
                    // re-reads them as opcodes and desyncs the stream
            break;
        case 0xA0: // write image: inline texture upload inside the display
                   // list. MGS's libdg streams stage textures this way, so an
                   // unhandled A0 leaves its pixel payload in the stream and
                   // the parser reads pixels as opcodes -- the screen fills
                   // with marching diagonal garbage as the queue desyncs.
        {
            RECT wrect;
            u32 words;
            wrect.x = (short)(queue_buf[i + 1] & 0x3FF);
            wrect.y = (short)((queue_buf[i + 1] >> 16) & 0x1FF);
            wrect.w = (short)(queue_buf[i + 2] & 0x7FF);
            wrect.h = (short)((queue_buf[i + 2] >> 16) & 0x3FF);
            words = ((u32)wrect.w * (u32)wrect.h + 1) / 2;
            if (wrect.w > 0 && wrect.h > 0 && i + 2 + (int)words < queue_len) {
                Draw_LoadImage(&wrect, (u_long*)&queue_buf[i + 3]);
            }
            i += 2 + (int)words; // consume header + pixel payload
            break;
        }
        case 0xC0: // read image
            GPU_read_image();
            break;
        case 0xE1:
            Draw_SetTexpageMode((ParamDrawTexpageMode*)&op);
            break;
        case 0xE2:
            // https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gp0e2h-texture-window-setting
            Draw_SetTextureWindow(
                (op & 0x1F) * 8 - 1, ((op >> 5) & 0x1F) * 8 - 1,
                ((op >> 10) & 0x1F) * 8, ((op >> 15) & 0x1F) * 8);
            break;
        case 0xE3:
#ifdef ESP_PLATFORM
            { static int b = 6; if (b > 0) { b--;
                printf("[env] E3 area start %d,%d (raw %08lx)\n",
                       (int)op & 0x3FF, (int)(op >> 10) & 0x3FF,
                       (unsigned long)op); } }
#endif
            Draw_SetAreaStart((int)op & 0x3FF, (int)(op >> 10) & 0x3FF);
            break;
        case 0xE4:
#ifdef ESP_PLATFORM
            { static int b = 6; if (b > 0) { b--;
                printf("[env] E4 area end %d,%d (raw %08lx)\n",
                       (int)op & 0x3FF, (int)(op >> 10) & 0x3FF,
                       (unsigned long)op); } }
#endif
            Draw_SetAreaEnd((int)op & 0x3FF, (int)(op >> 10) & 0x3FF);
            break;
        case 0xE5:
#ifdef ESP_PLATFORM
            { static int b = 6; if (b > 0) { b--;
                printf("[env] E5 offset %d,%d (raw %08lx)\n",
                       (int)op & 0x7FF, (int)(op >> 11) & 0x7FF,
                       (unsigned long)op); } }
#endif
            Draw_SetOffset((int)op & 0x7FF, (int)(op >> 11) & 0x7FF);
            break;
        case 0xE6:
            Draw_SetMask(!!(op & 1), !!(op & 2));
            break;
        default:
            if (code >= 0x20 && code < 0x80) {
#ifdef ESP_PLATFORM
                {
                    /* census of what actually reaches the rasterizer, and
                     * the screen coords of the first few polys */
                    extern unsigned psyz_prim_by_code[8];
                    static int shown = 8;
                    psyz_prim_by_code[(code >> 4) & 7]++;
                    if (shown > 0 && code >= 0x20 && code < 0x40) {
                        shown--;
                        const short* w = (const short*)&queue_buf[i + 1];
                        printf("[prim] code %02X xy %d,%d %d,%d\n", code,
                               w[0], w[1], w[2], w[3]);
                    }
                    /* Who painted the whole screen?
                     *
                     * An untextured polygon big enough to cover most of the
                     * frame is never scenery -- it is a flash, a fade or a
                     * curtain. When one of those comes out opaque white the
                     * picture is gone and the cause is invisible from the
                     * panel, so name it here: its colour says whether the
                     * source is white to begin with, and bit 1 of the code is
                     * ABE, the semi-transparency enable. A flash meant to be
                     * blended that arrives with ABE clear is drawn solid, and
                     * that alone turns a lighting effect into a blank screen.
                     *
                     * Flat primitives only (0x20-0x2F): their layout is one
                     * colour word then plain XY pairs, so the vertices can be
                     * read without decoding the gouraud interleave. */
                    {
                        /* The vertex stride is not fixed: a polygon packet is
                         * one colour word and then, per vertex, XY plus a UV
                         * word if textured plus a colour word if gouraud. My
                         * first version assumed plain XY pairs and read the
                         * next primitive's PSRAM pointer as a coordinate --
                         * hence boxes like 24671,15472. Derive the stride from
                         * the code bits instead: 0x10 gouraud, 0x08 quad,
                         * 0x04 textured, 0x02 semi-transparent. */
                        static int big = 12;
                        const unsigned long* p = &queue_buf[i];
                        int lo_x = 32767, hi_x = -32768;
                        int lo_y = 32767, hi_y = -32768;

                        if (code < 0x40) {
                            int stride = 1 + ((code & 0x04) ? 1 : 0) +
                                         ((code & 0x10) ? 1 : 0);
                            int n = (code & 0x08) ? 4 : 3;
                            int v;
                            for (v = 0; v < n; v++) {
                                const short* xy =
                                    (const short*)&p[1 + (long)v * stride];
                                if (xy[0] < lo_x) lo_x = xy[0];
                                if (xy[0] > hi_x) hi_x = xy[0];
                                if (xy[1] < lo_y) lo_y = xy[1];
                                if (xy[1] > hi_y) hi_y = xy[1];
                            }
                        } else if (code >= 0x60) {
                            /* Rectangles are a different packet entirely --
                             * colour, top-left, then a size word only when the
                             * code says variable-size (bits 4-3 zero; 1/2/3
                             * mean 1x1, 8x8, 16x16). A textured one slips a UV
                             * word in between. Reading the size as if it were a
                             * second vertex is what produced boxes like
                             * 0,-7424. A full-screen fill is most likely to
                             * arrive as exactly this. */
                            const short* xy = (const short*)&p[1];
                            int sz = (code >> 3) & 3;
                            int w = sz == 2 ? 8 : sz == 3 ? 16 : 1;
                            int h = w;
                            if (sz == 0) {
                                const short* wh = (const short*)
                                    &p[2 + ((code & 0x04) ? 1 : 0)];
                                w = wh[0];
                                h = wh[1];
                            }
                            lo_x = xy[0];
                            lo_y = xy[1];
                            hi_x = xy[0] + w;
                            hi_y = xy[1] + h;
                        }
                        /* The opaque black full-screen rectangle is the routine
                         * background fill, once or twice per frame. Reporting
                         * it would burn the quota long before anything
                         * interesting happens, and what we are hunting only
                         * shows up minutes into a session. Stay quiet for it. */
                        /* Textured primitives are IN. They were excluded on the
                         * reasoning that a white flash carries no texture --
                         * which was wrong, and excluded the very thing being
                         * hunted: a searchlight cone is a textured quad, and
                         * the frame it appears in pays for three full screens
                         * of texels. Instead of narrowing by kind, dedupe by
                         * (code, colour) so a cone reported once does not
                         * report again every frame. */
                        static unsigned seen_key[16];
                        static int      n_key;
                        unsigned key = (unsigned)code << 24 |
                                       (p[0] & 0xFFFFFFu);
                        int k, fresh = 1;
                        for (k = 0; k < n_key; k++)
                            if (seen_key[k] == key) { fresh = 0; break; }

                        if (big > 0 && fresh && (hi_x - lo_x) >= 240 &&
                            (hi_y - lo_y) >= 160 &&
                            !(code == 0x60 && (p[0] & 0xFFFFFF) == 0)) {
                            unsigned long c = p[0];
                            big--;
                            if (n_key < 16) seen_key[n_key++] = key;
                            printf("[cover] code %02X abe %d tex %d rgb "
                                   "%02lX%02lX%02lX box %d,%d..%d,%d\n",
                                   code, (code >> 1) & 1, (code >> 2) & 1,
                                   c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF,
                                   lo_x, lo_y, hi_x, hi_y);
                        }
                    }
                }
#endif
                i += Draw_PushPrim(&queue_buf[i], queue_len - i) - 1;
                break;
            }
            WARNF("unsupported command %02X", code);
#ifdef ESP_PLATFORM
            {
                // stream forensics: an unknown opcode usually means the
                // stream desynced a few words earlier - dump the window
                static int dumps;
                if (dumps < 4) {
                    dumps++;
                    int lo = i >= 8 ? i - 8 : 0;
                    int hi = i + 8 <= queue_len ? i + 8 : queue_len;
                    printf("gpuq desync @%d/%d:", i, queue_len);
                    for (int k = lo; k < hi; k++) {
                        printf(" %08lx%s", (unsigned long)queue_buf[k],
                               k == i ? "*" : "");
                    }
                    printf("\n");
                }
            }
#endif
        }
    }
    Draw_FlushBuffer();
    Draw_ExequeSync();
    queue_len = 0;
#ifdef ESP_PLATFORM
    { extern long long esp_timer_get_time(void);
      psyz_prof_exeq_us += (unsigned)(esp_timer_get_time() - psyz_prof_t0); }
#endif
    return queue_len;
}

#ifdef ESP_PLATFORM
// A corrupt chain link must not walk the parser into unmapped space: prims
// legitimately live in PSRAM (game buffers) or internal DRAM statics.
static inline int prim_ptr_ok(const void* p) {
    // mapped windows only: flash-rodata + the 8MB PSRAM (ends ~0x3C830000)
    // and internal DRAM statics; anything else MMU-faults on access
    uintptr_t a = (uintptr_t)p;
    return (a >= 0x3C000000u && a < 0x3C830000u) ||
           (a >= 0x3FC80000u && a < 0x3FD00000u);
}
#else
#define prim_ptr_ok(p) 1
#endif

static int GPU_Enqueue(u_long p1, u_long p2) {
    int mask = (int)p2;
    if (mask) {
        WARNF("mask not supported (mask:%08X)", mask);
    }
    DR_ENV* env = (DR_ENV*)(uintptr_t)p1;
    DR_ENV* prev_env = NULL;
    while (1) {
        if (!prim_ptr_ok(env)) {
            WARNF("prim chain walked to %p (bad link stored at %p) - rest "
                  "of OT dropped",
                  (void*)env, (void*)prev_env);
            break;
        }
        prev_env = env;
#ifdef ESP_PLATFORM
        if ((getcode(env) & 0xE0) == 0 && env->len > 2) {
            // a multi-word packet with command byte 0: not a legal prim.
            // Its ADDRESS names the producer via the symbol table.
            static int zero_log = 8;
            if (zero_log > 0) {
                zero_log--;
                u32* w = (u32*)env;
                printf("[gpu] code-0 prim at %p:", (void*)env);
                for (int k = 0; k < 14; k++) {
                    printf(" %08lx", (unsigned long)w[k]);
                }
                printf("\n");
            }
        }
#endif
        if (env->len > 0x100) {
            // no legit primitive is this long: drop it WITHOUT advancing
            // queue_len, or the queue walks off into unmapped memory.
            // Logging every drop stalls the frame on slow consoles - count
            // instead (surfaced in the ESP frame telemetry).
#ifdef ESP_PLATFORM
            {
                extern unsigned sotn_pkt_drops;
                static int drop_log = 12;
                sotn_pkt_drops++;
                if (drop_log > 0) {
                    drop_log--;
                    WARNF("dropped pkt at %p: len 0x%X code 0x%02X",
                          (void*)env, (unsigned)env->len,
                          (unsigned)(getcode(env) & 0xFF));
                }
            }
#else
            ERRORF("packet 0x%X long, likely corrupted - dropped", env->len);
#endif
            if (isendprim(env)) {
                break;
            }
            env = (DR_ENV*)nextPrim(env);
            continue;
        }
        if (queue_len + env->len > LEN(queue_buf)) {
            // drain and RETRY this packet: falling through without copying
            // would still bump queue_len and enqueue garbage words
            INFOF("GPU queue full, calling exeque");
            Psyz_GpuExeque();
            continue;
        }
        if (sizeof(u_long) == 4) {
            // this is fine on 32-bit systems
            memcpy(queue_buf + queue_len, env->code, env->len * sizeof(u_long));
        } else if (sizeof(u_long) == 8) {
            // Wow okay, this part is uuuugly...
            // Gpu code is usually written to a u_long array, which will work
            // fine on both 32-bit and 64-bit compiled code.
            // But primitives are mapped from structs, we need to align the data
            int code = getcode(env) & ~3;
            if (code >= 0x20 && code < 0x80) {
                // it is a prim, we need to split
                u32* prim_data = (u32*)env->code;
                for (u_long i = 0; i < env->len; i++) {
                    queue_buf[queue_len + i] = prim_data[i];
                }
            } else if (env->len > 0) {
                // TODO this is a temporary solution:
                // if gpu commands get merged with primitives, this will not
                // work
                memcpy(queue_buf + queue_len, env->code,
                       env->len * sizeof(u_long));
            }
        }
        queue_len += (int)env->len;
        if (isendprim(env)) {
            break;
        }
        env = (DR_ENV*)nextPrim(env);
    }
    return 0;
}
static int GPU_DataWrite(u_long p1, u_long p2) {
#ifdef ESP_PLATFORM
    extern long long esp_timer_get_time(void);
    long long t = esp_timer_get_time();
#endif
    Psyz_GpuExeque();
    Draw_LoadImage((RECT*)(uintptr_t)p1, (u_long*)(uintptr_t)p2);
#ifdef ESP_PLATFORM
    psyz_prof_load_us += (unsigned)(esp_timer_get_time() - t);
    psyz_prof_load_n++;
#endif
    return 0;
}
static int GPU_DataRead(u_long p1, u_long p2) {
    Psyz_GpuExeque();
    Draw_StoreImage((RECT*)(uintptr_t)p1, (u_long*)(uintptr_t)p2);
    return 0;
}

static int _param(int x) { return 0; }
static int psyz_addque2(
    int (*exec)(u_long p1, u_long p2), u_long p1, int len, u_long p2) {
    return exec(p1, p2);
}
static int psyz_addque(
    int (*exec)(u_long p1, u_long p2), u_long p1, u_long p2) {
    return psyz_addque2(exec, p1, 0, p2);
}

// psyz_clr is very similar to _clr from libgpu/sys.c
static DR_ENV clear_cmd;
static int psyz_clr(RECT* rect, u32 color) {
    const u32 wh = get_vram_wh();
    const unsigned short w = wh & 0xFFFF;
    const unsigned short h = wh >> 16;
    rect->w = CLAMP(rect->w, 0, w - 1);
    rect->h = CLAMP(rect->h, 0, h - 1);

    setlen(&clear_cmd, 5);
    clear_cmd.code[0] = 0xE6000000; // mask bit setting
    clear_cmd.code[1] = 0xE1000000 | GPU_STATUS & 0x7FF | (color >> 0x1F) << 10;
    clear_cmd.code[2] = (color & 0xFFFFFF) | 0x02000000;
    clear_cmd.code[3] = (u_long) * (u32*)&rect->x;
    clear_cmd.code[4] = (u_long) * (u32*)&rect->w;
    termPrim(&clear_cmd);
    GPU_Enqueue((u_long)&clear_cmd, 0);
    return 0;
}

void Psyz_GpuDisplayCommand(unsigned int cmd) {
    unsigned char op = (cmd >> 24) & 0x3F;
    switch (op) {
    case 0:
        Draw_Reset();
        break;
    case 1:
        LOG_ONCE("Reset FIFO not implemented");
        break;
    case 2:
        LOG_ONCE("Ack IRQ not implemented");
        break;
    case 3:
        Draw_DisplayEnable(!(cmd & 1));
        break;
    case 4:
        LOG_ONCE("DMA direction not implemented");
        break;
    case 5:
        Draw_DisplayArea(cmd & 0x3FF, (cmd >> 10) & 0x3FF);
        break;
    case 6:
        Draw_DisplayHorizontalRange(cmd & 0xFFF, (cmd >> 12) & 0xFFF);
        break;
    case 7:
        Draw_DisplayVerticalRange(cmd & 0x3FF, (cmd >> 10) & 0x3FF);
        break;
    case 8:
        cmd &= 0xFFFFFF;
        Draw_SetDisplayMode((DisplayMode*)&cmd);
        break;
    default:
        WARNF("unhandled ctl %02X (%08X)", op, cmd);
        break;
    }
}
static int psyz_cwb() {
    NOT_IMPLEMENTED;
    return 0;
}
static int psyz_getctl(int _) {
    NOT_IMPLEMENTED;
    return 0;
}
static void psyz_otc(OT_TYPE* ot, s32 n) {
    // The PSX built the reverse-linked table with the OTC DMA (channel 6); this
    // is that DMA in software: entry i links to entry i-1 and entry 0 carries
    // the terminator (ClearOTagR's caller may re-link entry 0 afterwards).
    // SOTN never hits this path -- it clears forward with ClearOTag -- so the
    // stub only became load-bearing with MGS, whose libdg clears reversed.
    s32 i;
    for (i = n - 1; i > 0; i--) {
        setlen(ot + i, 0);
        setaddr(ot + i, ot + i - 1);
    }
    setlen(ot, 0);
    setaddr(ot, 0xFFFFFF);
}
static int psyz_param(int _) {
    NOT_IMPLEMENTED;
    return 0;
}
static int psyz_reset(int _) { return GPU_V0; }
static u_long psyz_status(void) {
    NOT_IMPLEMENTED;
    return 0;
}
static int psyz_sync(int mode) {
    // see decomp/src/libgpu/sys.c
    // mode 0 waits until all the queue is drawn on screen
    // mode 1 process the queue and return how many elements have been queued
    // return -1 if GPU has timed out
    // but on PC the implementation is much simpler as it's always synced
    Psyz_GpuExeque();
    return 0;
}

int psyz_gpu_version(int mode) { return GPU_V0; }

// forwards raw GP0 words into the internal queue, then trigger execution.
void Psyz_GpuWriteGP0(unsigned int word) {
    if (queue_len >= (int)LEN(queue_buf)) {
        WARNF("GPU queue full");
        return;
    }
    queue_buf[queue_len++] = (u_long)word;
}

void GPU_cw(u_long* param) {
    struct Gpu* gpu = (struct Gpu*)param;
    gpu->ver = "psyz";
    gpu->addque = psyz_addque;
    gpu->addque2 = psyz_addque2;
    gpu->clr = psyz_clr;
    gpu->ctl = Psyz_GpuDisplayCommand;
    gpu->cwb = psyz_cwb;
    gpu->cwc = GPU_Enqueue;
    gpu->drs = GPU_DataRead;
    gpu->dws = GPU_DataWrite;
    gpu->exeque = Psyz_GpuExeque;
    gpu->getctl = psyz_getctl;
    gpu->otc = psyz_otc;
    gpu->param = psyz_param;
    gpu->reset = psyz_reset;
    gpu->status = psyz_status;
    gpu->sync = psyz_sync;
}

// these are not yet decompiled
int _addque2() { return 0; }
int _exeque() { return 0; }
int get_alarm(void) { return 0; }
