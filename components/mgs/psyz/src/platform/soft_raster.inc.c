// PSY-Z software rasterizer - portable core.
//
// Plain C over a u16[512][1024] VRAM in native PSX RGB5551. No SDL, no OS, no
// allocations: this translation unit is compiled unchanged into both the PC
// harness (soft_gpu.c includes it) and the ESP32-S3 firmware. Anything
// platform-shaped belongs in the including shell, not here.
//
// It is written as an includable .inc.c on purpose: the shell provides `vram`
// resolution and the packet parser, the core provides state + rasterization +
// VRAM block ops, and both compile as ONE unit so the hot paths inline.

// ---- shared-macro shims -----------------------------------------------------
// When the PC shell includes sdl3_common.h first it already provides these;
// the ESP32 build defines SOFT_RASTER_STANDALONE and gets them here.
#ifdef SOFT_RASTER_STANDALONE
#include <stdbool.h>
// PS1_RECT is sdl3_common's rename of libgpu's RECT (dodging windef's on
// Windows); standalone builds have no windef to dodge.
#define PS1_RECT RECT
#define SEMITRANSP 0x02
#define TEXTURED 0x04
#define EXTRA_VERTEX 0x08
#define GOURAUD 0x10
#define TRIANGLE 0x20
static unsigned short cur_tpage = 0;
static short s11(short v) { return (short)(((v & 0x7FF) ^ 1024) - 1024); }
#ifndef WARNF
#define WARNF(...)
#endif
void Draw_SetTexpageMode(ParamDrawTexpageMode* p) {
    cur_tpage = (*(unsigned short*)p) & 0x1FF;
}
void Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                           unsigned int off_x, unsigned int off_y) {
    (void)mask_x; (void)mask_y; (void)off_x; (void)off_y;
}
void Draw_SetMask(int bit0, int bit1) { (void)bit0; (void)bit1; }
#endif // SOFT_RASTER_STANDALONE

// ==================== core (SDL-free, ESP32-ready) ====================

// The game's VRAM when linked into SOTN; local fallback otherwise. Declared
// weak so psyz still links standalone.
#ifdef SOFT_RASTER_NO_FALLBACK
// The embedding shell owns VRAM placement outright (the ESP32 build puts it in
// PSRAM with an explicit attribute); a megabyte of fallback .bss would not
// even link against 512KB of internal RAM.
#elif defined(_MSC_VER)
static u16 soft_vram_fallback[VRAM_H * VRAM_W];
#define SOFT_VRAM soft_vram_fallback
#else
extern u16 g_RawVram[VRAM_H * VRAM_W] __attribute__((weak));
static u16 soft_vram_fallback[VRAM_H * VRAM_W];
#define SOFT_VRAM (g_RawVram ? g_RawVram : soft_vram_fallback)
#endif

static u16* vram; // resolved once in InitPlatform

typedef struct {
    int x0, y0, x1, y1; // draw area, inclusive
    int ox, oy;         // draw offset
} SoftState;
static SoftState st = {0, 0, VRAM_W - 1, VRAM_H - 1, 0, 0};

// One vertex after packet parsing. Colors stay 8-bit until the pixel write.
typedef struct {
    int x, y;
    int u, v;
    int r, g, b;
} SV;

// The embedded build attributes the rasterizer loops into IRAM and swaps the
// PSRAM CLUT pointer for a small internal-SRAM cache; on PC both default off.
#ifndef SOFT_RASTER_IRAM
#define SOFT_RASTER_IRAM
#endif

#ifdef SOFT_CLUT_CACHE
// 8-way store of recently used CLUTs (4KB DRAM). Texel fetches then cost one
// PSRAM read instead of two. Any VRAM image op invalidates everything - CLUT
// uploads and palette animation go through LoadImage.
static u16 clut_cache[8][256];
static u16 clut_cache_key[8] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
                                0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
static u8 clut_cache_next;
static void clut_cache_flush(void) {
    for (int i = 0; i < 8; i++) {
        clut_cache_key[i] = 0xFFFF;
    }
}
static inline const u16* clut_cached(u16 id, const u16* src, int n) {
    for (int i = 0; i < 8; i++) {
        if (clut_cache_key[i] == id) {
            return clut_cache[i];
        }
    }
    int slot = clut_cache_next++ & 7;
    memcpy(clut_cache[slot], src, n * sizeof(u16));
    clut_cache_key[slot] = id;
    return clut_cache[slot];
}
#else
#define clut_cache_flush()
#endif

// ---- pixel write -----------------------------------------------------------

// Semi-transparency modes, tpage bits 5-6. B = background (dest), F = front.
//   0: B/2 + F/2    1: B + F    2: B - F    3: B + F/4
static inline u16 blend5551(u16 back, int fr5, int fg5, int fb5, int abr) {
    int br = back & 0x1F, bg = (back >> 5) & 0x1F, bb = (back >> 10) & 0x1F;
    int r, g, b;
    switch (abr) {
    case 0:
        r = (br + fr5) >> 1;
        g = (bg + fg5) >> 1;
        b = (bb + fb5) >> 1;
        break;
    case 1:
        r = br + fr5;
        g = bg + fg5;
        b = bb + fb5;
        break;
    case 2:
        r = br - fr5;
        g = bg - fg5;
        b = bb - fb5;
        break;
    default:
        r = br + (fr5 >> 2);
        g = bg + (fg5 >> 2);
        b = bb + (fb5 >> 2);
        break;
    }
    if (r < 0) r = 0; else if (r > 31) r = 31;
    if (g < 0) g = 0; else if (g > 31) g = 31;
    if (b < 0) b = 0; else if (b > 31) b = 31;
    return (u16)(r | (g << 5) | (b << 10));
}

// PS1 texture modulation: out5 = min(31, tex5 * col8 >> 7). 0x80 is neutral.
static inline int mod5(int tex5, int col8) {
    int v = (tex5 * col8) >> 7;
    return v > 31 ? 31 : v;
}

// ---- texture fetch ---------------------------------------------------------

typedef struct {
    const u16* page;  // top-left of the texture page in VRAM
    const u16* clut;  // 16 or 256 entries
    int bpp;          // 0=4bpp, 1=8bpp, 2=15bpp
    int abr;          // semi-transparency mode from the tpage
} Tex;

static inline Tex tex_from(u16 tpage, u16 clut) {
    Tex t;
    int px = (tpage & 0xF) * 64;         // halfword units
    int py = ((tpage >> 4) & 1) * 256;
    t.page = vram + py * VRAM_W + px;
    t.bpp = (tpage >> 7) & 3;
    t.abr = (tpage >> 5) & 3;
    t.clut = vram + ((clut >> 6) & 0x1FF) * VRAM_W + (clut & 0x3F) * 16;
#ifdef SOFT_CLUT_CACHE
    if (t.bpp <= 1) {
        t.clut = (u16*)clut_cached(clut, t.clut, t.bpp == 0 ? 16 : 256);
    }
#endif
    return t;
}

static inline u16 tex_fetch(const Tex* t, int u, int v) {
    u &= 0xFF;
    v &= 0xFF;
    switch (t->bpp) {
    case 0: { // 4bpp: 4 texels per halfword
        u16 hw = t->page[v * VRAM_W + (u >> 2)];
        return t->clut[(hw >> ((u & 3) * 4)) & 0xF];
    }
    case 1: { // 8bpp: 2 texels per halfword
        u16 hw = t->page[v * VRAM_W + (u >> 1)];
        return t->clut[(hw >> ((u & 1) * 8)) & 0xFF];
    }
    default: // 15bpp direct
        return t->page[v * VRAM_W + u];
    }
}

// ---- primitive rasterizers -------------------------------------------------

// Shared per-pixel tail for textured pixels. Returns without writing on the
// PSX "texel 0000 is never drawn" rule; blends only texels with the STP bit
// when the primitive is semi-transparent.
#ifdef ESP_PLATFORM
unsigned psyz_texel_zero, psyz_texel_ok, psyz_flat_px;
unsigned psyz_tri_off, psyz_tri_tiny, psyz_tri_ok;
/* polygons the PSX GPU would have refused for exceeding 1023x511 */
unsigned psyz_tri_huge;
#endif
static inline void put_texel(u16* dst, u16 texel, int r8, int g8, int b8,
                             int semi, int abr) {
    if (texel == 0) {
#ifdef ESP_PLATFORM
        psyz_texel_zero++;
#endif
        return;
    }
#ifdef ESP_PLATFORM
    psyz_texel_ok++;
#endif
    // 0x80 modulation is neutral: mod5(t,128)==t, so the whole mul/clamp
    // chain collapses to a passthrough (bit-exact with the general form)
    if (r8 == 0x80 && g8 == 0x80 && b8 == 0x80) {
        if (semi && (texel & 0x8000)) {
            *dst = blend5551(*dst, texel & 0x1F, (texel >> 5) & 0x1F,
                             (texel >> 10) & 0x1F, abr);
        } else {
            *dst = texel;
        }
        return;
    }
    int r = mod5(texel & 0x1F, r8);
    int g = mod5((texel >> 5) & 0x1F, g8);
    int b = mod5((texel >> 10) & 0x1F, b8);
    if (semi && (texel & 0x8000)) {
        *dst = blend5551(*dst, r, g, b, abr);
    } else {
        *dst = (u16)(r | (g << 5) | (b << 10) | (texel & 0x8000));
    }
}

static inline void put_flat(u16* dst, int r8, int g8, int b8, int semi,
                            int abr) {
#ifdef ESP_PLATFORM
    psyz_flat_px++;
#endif
    int r = r8 >> 3, g = g8 >> 3, b = b8 >> 3;
    if (semi) {
        *dst = blend5551(*dst, r, g, b, abr);
    } else {
        *dst = (u16)(r | (g << 5) | (b << 10));
    }
}

// Axis-aligned textured sprite (SPRT/SPRT_8/SPRT_16). PSX-exact sampling:
// u advances one texel per pixel, no interpolation.
SOFT_RASTER_IRAM static void raster_sprite(const SV* v, int w, int h, u16 tpage, u16 clut,
                          int semi) {
    Tex t = tex_from(tpage, clut);
    int x0 = v->x + st.ox, y0 = v->y + st.oy;
    // clip once instead of testing every pixel
    int i0 = 0, i1 = w - 1, j0 = 0, j1 = h - 1;
    if (x0 + i0 < st.x0) i0 = st.x0 - x0;
    if (x0 + i1 > st.x1) i1 = st.x1 - x0;
    if (y0 + j0 < st.y0) j0 = st.y0 - y0;
    if (y0 + j1 > st.y1) j1 = st.y1 - y0;
    if (i0 > i1 || j0 > j1) {
        return;
    }
    for (int j = j0; j <= j1; j++) {
        u16* row = vram + (y0 + j) * VRAM_W;
        int tv = (v->v + j) & 0xFF;
        int i = i0;
        if (t.bpp == 0) {
            // 4bpp: one halfword read serves four texels
            const u16* trow = t.page + tv * VRAM_W;
            // Tilemap fast path: opaque, neutral modulation, u aligned to a
            // halfword. Four texels per source read, pixel pairs written as
            // one 32-bit store. Per-texel semantics are identical to the
            // general path (texel 0 stays transparent, neutral mod is a
            // passthrough), so output is bit-for-bit the same.
            if (!semi && v->r == 0x80 && v->g == 0x80 && v->b == 0x80 &&
                ((v->u + i0) & 3) == 0 && ((i1 - i0) & 3) == 3) {
                u16* dst = &row[x0 + i0];
                for (int n = i0; n <= i1; n += 4) {
                    u16 hw = trow[((v->u + n) & 0xFF) >> 2];
                    u16 a = t.clut[hw & 0xF];
                    u16 b = t.clut[(hw >> 4) & 0xF];
                    u16 c = t.clut[(hw >> 8) & 0xF];
                    u16 d = t.clut[(hw >> 12) & 0xF];
                    if (a && b) {
                        *(u32*)dst = (u32)a | ((u32)b << 16);
                    } else {
                        if (a) dst[0] = a;
                        if (b) dst[1] = b;
                    }
                    if (c && d) {
                        *(u32*)(dst + 2) = (u32)c | ((u32)d << 16);
                    } else {
                        if (c) dst[2] = c;
                        if (d) dst[3] = d;
                    }
                    dst += 4;
                }
                continue;
            }
            while (i <= i1) {
                int u = (v->u + i) & 0xFF;
                u16 hw = trow[u >> 2];
                int nib = u & 3;
                do {
                    put_texel(&row[x0 + i], t.clut[(hw >> (nib * 4)) & 0xF],
                              v->r, v->g, v->b, semi, t.abr);
                    i++;
                    nib++;
                } while (nib < 4 && i <= i1);
            }
        } else {
            for (; i <= i1; i++) {
                put_texel(&row[x0 + i], tex_fetch(&t, v->u + i, tv), v->r,
                          v->g, v->b, semi, t.abr);
            }
        }
    }
}

// Flat rectangle (TILE family).
SOFT_RASTER_IRAM static void raster_tile(const SV* v, int w, int h, int semi, int abr) {
    int x0 = v->x + st.ox, y0 = v->y + st.oy;
    int xa = x0 < st.x0 ? st.x0 : x0;
    int xb = x0 + w - 1 > st.x1 ? st.x1 : x0 + w - 1;
    int ya = y0 < st.y0 ? st.y0 : y0;
    int yb = y0 + h - 1 > st.y1 ? st.y1 : y0 + h - 1;

    /* Opaque fill: nothing to read, nothing to interpolate, one colour for
     * every pixel -- so the general per-pixel path is all overhead. It matters
     * far more than a tile normally would, because MGS clears the frame with
     * ONE 320x224 tile: 71,680 pixels, more than half of everything the
     * rasterizer touches in a frame, each costing a branch and a single 16-bit
     * store into PSRAM. Writing pairs as one 32-bit store halves the store
     * count and lets the memory bus move a full word per transaction. Output
     * is bit-for-bit what put_flat produced. */
    if (!semi) {
        u16 c = (u16)((v->r >> 3) | ((v->g >> 3) << 5) | ((v->b >> 3) << 10));
        u32 cc = (u32)c | ((u32)c << 16);
        for (int y = ya; y <= yb; y++) {
            u16* p = vram + y * VRAM_W + xa;
            int n = xb - xa + 1;
#ifdef ESP_PLATFORM
            psyz_flat_px += (unsigned)(n > 0 ? n : 0);
#endif
            if ((xa & 1) && n > 0) {   /* odd start: one halfword to align */
                *p++ = c;
                n--;
            }
            {
                u32* q = (u32*)p;
                for (int k = n >> 1; k > 0; k--) {
                    *q++ = cc;
                }
                if (n & 1) {
                    *(u16*)q = c;
                }
            }
        }
        return;
    }

    for (int y = ya; y <= yb; y++) {
        u16* row = vram + y * VRAM_W;
        for (int x = xa; x <= xb; x++) {
            put_flat(&row[x], v->r, v->g, v->b, semi, abr);
        }
    }
}

// Triangle with gouraud + affine UV, integer edge functions, top-left rule.
// 2D game, orthographic: affine interpolation is exact.
SOFT_RASTER_IRAM static void raster_tri(const SV* a, const SV* b, const SV* c, int textured,
                       u16 tpage, u16 clut, int semi, int gouraud) {
    int ax = a->x + st.ox, ay = a->y + st.oy;
    int bx = b->x + st.ox, by = b->y + st.oy;
    int cx = c->x + st.ox, cy = c->y + st.oy;

    int area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (area == 0) {
        return;
    }
    if (area < 0) { // enforce CCW so the edge functions face inward
        const SV* t = b;
        b = c;
        c = t;
        int ti = bx; bx = cx; cx = ti;
        ti = by; by = cy; cy = ti;
        area = -area;
    }

#ifdef ESP_PLATFORM
    {
        /* one-shot: where is the rasterizer actually writing, and what is it
         * clipping to? Compare against the framebuffer the scanout reads. */
#ifdef MGS_ESPBOX
        extern unsigned short* g_RawVram;
#else
        extern unsigned short g_RawVram[];
#endif
        static int shown = 3;
        if (shown > 0) {
            shown--;
            printf("[rast] vram %p g_RawVram %p ofs %d,%d clip %d,%d..%d,%d "
                   "tri %d,%d %d,%d %d,%d\n",
                   (void*)vram, (void*)g_RawVram, st.ox, st.oy,
                   st.x0, st.y0, st.x1, st.y1, ax, ay, bx, by, cx, cy);
        }
    }
#endif
    int minx = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
    int maxx = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
    int miny = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
    int maxy = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
#ifdef ESP_PLATFORM
    {
        /* classify: is the world missing because it lands outside the screen,
         * or because it projects to sub-pixel size? */
        extern unsigned psyz_tri_off, psyz_tri_tiny, psyz_tri_ok;
        if (maxx < st.x0 || minx > st.x1 || maxy < st.y0 || miny > st.y1) {
            psyz_tri_off++;
        } else if ((maxx - minx) < 2 && (maxy - miny) < 2) {
            psyz_tri_tiny++;
        } else {
            static int shown = 8;
            psyz_tri_ok++;
            if (shown > 0) {
                shown--;
                printf("[ok] tri (%d,%d) (%d,%d) (%d,%d) area %d tex %d\n",
                       ax, ay, bx, by, cx, cy, area, textured);
            }
        }
    }
#endif
    /* The PSX GPU refuses any polygon wider than 1023 or taller than 511, and
     * that refusal is load-bearing rather than a curiosity. When a polygon
     * crosses behind the camera its projection is meaningless and the GTE pins
     * the offending vertices at its +/-1024 clamp, producing a quad that
     * stretches from one clamp corner across the whole screen. The console
     * simply drops those. Without the rule a software rasterizer draws them at
     * full size: on this board a searchlight cone came out as a moving white
     * sheet over the dock, with the HUD and the credit text still legible on
     * top of it because they are drawn afterwards -- three full screens of
     * texels per frame and 142 ms to raster.
     *
     * Measured on the RAW extents, before clipping to the drawing area: the
     * clip would shrink every one of them to screen size and hide exactly the
     * property being tested. */
    if ((maxx - minx) > 1023 || (maxy - miny) > 511) {
#ifdef ESP_PLATFORM
        extern unsigned psyz_tri_huge;
        psyz_tri_huge++;
#endif
        return;
    }

    if (minx < st.x0) minx = st.x0;
    if (maxx > st.x1) maxx = st.x1;
    if (miny < st.y0) miny = st.y0;
    if (maxy > st.y1) maxy = st.y1;
    if (minx > maxx || miny > maxy) {
        return;
    }

    // edge setup: E(x,y) >= 0 inside, top-left edges get the <0 bias on the
    // others so shared edges paint exactly once
    int A0 = by - cy, B0 = cx - bx, C0 = bx * cy - by * cx;
    int A1 = cy - ay, B1 = ax - cx, C1 = cx * ay - cy * ax;
    int A2 = ay - by, B2 = bx - ax, C2 = ax * by - ay * bx;
    int bias0 = (A0 > 0 || (A0 == 0 && B0 > 0)) ? 0 : -1;
    int bias1 = (A1 > 0 || (A1 == 0 && B1 > 0)) ? 0 : -1;
    int bias2 = (A2 > 0 || (A2 == 0 && B2 > 0)) ? 0 : -1;

    Tex t;
    if (textured) {
        t = tex_from(tpage, clut);
    }

    int w0row = A0 * minx + B0 * miny + C0 + bias0;
    int w1row = A1 * minx + B1 * miny + C1 + bias1;
    int w2row = A2 * minx + B2 * miny + C2 + bias2;

    // Per-triangle attribute gradients, 16.16 fixed point. Xtensa has no fast
    // divider, and the first-correctness version paid 3-5 integer divisions
    // per PIXEL for barycentric attributes - measured at 100-130 cy/px on the
    // S3, it dominated every gouraud primitive. An affine attribute is a plane
    // over (x,y): compute d/dx and d/dy once with a handful of divisions, then
    // step. The +area/2 rounds the gradient to nearest, which keeps the last
    // scanline of a 255-wide gradient from drifting versus the exact form.
#define GRAD(p0, p1, p2)                                                           dx_ = ((s64)(A0) * (p0) + (s64)(A1) * (p1) + (s64)(A2) * (p2)) * 65536;        dy_ = ((s64)(B0) * (p0) + (s64)(B1) * (p1) + (s64)(B2) * (p2)) * 65536;        dx_ = dx_ >= 0 ? (dx_ + area / 2) / area : (dx_ - area / 2) / area;            dy_ = dy_ >= 0 ? (dy_ + area / 2) / area : (dy_ - area / 2) / area;

    typedef long long s64;
    s64 dx_, dy_;
    int e0r = w0row - bias0, e1r = w1row - bias1, e2r = w2row - bias2;

    // Accumulators are 32-bit 16.16: attributes are 8-bit, spans are under
    // 1024px, so values stay far from overflow - and on a 32-bit Xtensa a
    // 64-bit accumulator costs two registers and multi-instruction adds per
    // pixel, which measured SLOWER than the hardware-divide version it was
    // meant to replace (17.8M vs 13.1M cy on the quad battery).
    typedef int32_t fx;
    fx rrow = 0, grow = 0, brow = 0, urow = 0, vrow = 0;
    fx rdx = 0, rdy = 0, gdx = 0, gdy = 0, bdx = 0, bdy = 0;
    fx udx = 0, udy = 0, vdx = 0, vdy = 0;
    if (gouraud) {
        GRAD(a->r, b->r, c->r) rdx = (fx)dx_; rdy = (fx)dy_;
        GRAD(a->g, b->g, c->g) gdx = (fx)dx_; gdy = (fx)dy_;
        GRAD(a->b, b->b, c->b) bdx = (fx)dx_; bdy = (fx)dy_;
        rrow = (fx)(((s64)(e0r * a->r + e1r * b->r + e2r * c->r) << 16) / area);
        grow = (fx)(((s64)(e0r * a->g + e1r * b->g + e2r * c->g) << 16) / area);
        brow = (fx)(((s64)(e0r * a->b + e1r * b->b + e2r * c->b) << 16) / area);
    }
    if (textured) {
        GRAD(a->u, b->u, c->u) udx = (fx)dx_; udy = (fx)dy_;
        GRAD(a->v, b->v, c->v) vdx = (fx)dx_; vdy = (fx)dy_;
        urow = (fx)(((s64)(e0r * a->u + e1r * b->u + e2r * c->u) << 16) / area);
        vrow = (fx)(((s64)(e0r * a->v + e1r * b->v + e2r * c->v) << 16) / area);
    }
#undef GRAD

    // Fast path for the dominant WRP case: opaque untextured spans (the big
    // gouraud background quads). Span bounds come from the edge functions
    // directly (three divides per row on the LX7's hardware divider), the
    // inner loop steps only RGB and writes pixel pairs with 32-bit stores.
    // Attribute stepping is identical to the generic loop, so pixel output
    // is bit-exact with it.
    if (!textured && !semi) {
        fx r0 = gouraud ? rrow : (fx)(a->r << 16);
        fx g0 = gouraud ? grow : (fx)(a->g << 16);
        fx b0 = gouraud ? brow : (fx)(a->b << 16);
        fx frdx = gouraud ? rdx : 0, frdy = gouraud ? rdy : 0;
        fx fgdx = gouraud ? gdx : 0, fgdy = gouraud ? gdy : 0;
        fx fbdx = gouraud ? bdx : 0, fbdy = gouraud ? bdy : 0;
        for (int y = miny; y <= maxy; y++, w0row += B0, w1row += B1,
                 w2row += B2, r0 += frdy, g0 += fgdy, b0 += fbdy) {
            int xl = minx, xr = maxx;
            int we[3] = {w0row, w1row, w2row};
            int Ae[3] = {A0, A1, A2};
            int empty = 0;
            for (int e = 0; e < 3; e++) {
                int w = we[e], A = Ae[e];
                if (A == 0) {
                    if (w < 0) { empty = 1; break; }
                } else if (A > 0) {
                    if (w < 0) {
                        int x = minx + (-w + A - 1) / A;
                        if (x > xl) xl = x;
                    }
                } else {
                    if (w < 0) { empty = 1; break; }
                    int x = minx + w / (-A);
                    if (x < xr) xr = x;
                }
            }
            if (empty || xl > xr) {
                continue;
            }
            fx ri = r0 + (fx)(xl - minx) * frdx;
            fx gi = g0 + (fx)(xl - minx) * fgdx;
            fx bi = b0 + (fx)(xl - minx) * fbdx;
            u16* p = vram + y * VRAM_W + xl;
            int n = xr - xl + 1;
#define SPAN_PIX()                                                             \
    (u16)((((unsigned)ri >> 19) & 0x1F) | ((((unsigned)gi >> 19) & 0x1F) << 5) \
          | ((((unsigned)bi >> 19) & 0x1F) << 10))
            if (((uintptr_t)p & 2) && n) {
                *p++ = SPAN_PIX();
                ri += frdx; gi += fgdx; bi += fbdx;
                n--;
            }
            u32* p2 = (u32*)p;
            while (n >= 2) {
                u16 lo = SPAN_PIX();
                ri += frdx; gi += fgdx; bi += fbdx;
                u16 hi = SPAN_PIX();
                ri += frdx; gi += fgdx; bi += fbdx;
                *p2++ = (u32)lo | ((u32)hi << 16);
                n -= 2;
            }
            if (n) {
                *(u16*)p2 = SPAN_PIX();
            }
#undef SPAN_PIX
        }
        return;
    }

    // Generic loop, span-based: per row the edge functions give [xl, xr]
    // directly (three hardware divides), so the inner loop carries no edge
    // accumulators and no accept test - just attribute stepping + the pixel
    // write. Identical stepping start (xl offset from minx) keeps output
    // bit-exact with the per-pixel-test form.
    for (int y = miny; y <= maxy; y++, w0row += B0, w1row += B1, w2row += B2,
             rrow += rdy, grow += gdy, brow += bdy, urow += udy, vrow += vdy) {
        int xl = minx, xr = maxx;
        {
            int we[3] = {w0row, w1row, w2row};
            int Ae[3] = {A0, A1, A2};
            int empty = 0;
            for (int e = 0; e < 3; e++) {
                int w = we[e], A = Ae[e];
                if (A == 0) {
                    if (w < 0) { empty = 1; break; }
                } else if (A > 0) {
                    if (w < 0) {
                        int x = minx + (-w + A - 1) / A;
                        if (x > xl) xl = x;
                    }
                } else {
                    if (w < 0) { empty = 1; break; }
                    int x = minx + w / (-A);
                    if (x < xr) xr = x;
                }
            }
            if (empty || xl > xr) {
                continue;
            }
        }
        fx ri = rrow + (fx)(xl - minx) * rdx;
        fx gi = grow + (fx)(xl - minx) * gdx;
        fx bi = brow + (fx)(xl - minx) * bdx;
        fx ui = urow + (fx)(xl - minx) * udx;
        fx vi = vrow + (fx)(xl - minx) * vdx;
        u16* row = vram + y * VRAM_W;
        for (int x = xl; x <= xr; x++, ri += rdx, gi += gdx, bi += bdx,
                 ui += udx, vi += vdx) {
            int r8 = a->r, g8 = a->g, b8 = a->b;
            if (gouraud) {
                r8 = (int)(ri >> 16);
                g8 = (int)(gi >> 16);
                b8 = (int)(bi >> 16);
            }
            if (textured) {
                put_texel(&row[x],
                          tex_fetch(&t, (int)(ui >> 16), (int)(vi >> 16)), r8,
                          g8, b8, semi, t.abr);
            } else {
                put_flat(&row[x], r8, g8, b8, semi, (cur_tpage >> 5) & 3);
            }
        }
    }
}

// Gouraud line, Bresenham over the major axis.
SOFT_RASTER_IRAM static void raster_line(const SV* a, const SV* b, int semi) {
    int x0 = a->x + st.ox, y0 = a->y + st.oy;
    int x1 = b->x + st.ox, y1 = b->y + st.oy;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int steps = (dx > -dy ? dx : -dy);
    if (steps == 0) {
        steps = 1;
    }
    int abr = (cur_tpage >> 5) & 3;
    for (int i = 0;; i++) {
        if (x0 >= st.x0 && x0 <= st.x1 && y0 >= st.y0 && y0 <= st.y1) {
            int r = a->r + (b->r - a->r) * i / steps;
            int g = a->g + (b->g - a->g) * i / steps;
            int bl = a->b + (b->b - a->b) * i / steps;
            put_flat(&vram[y0 * VRAM_W + x0], r, g, bl, semi, abr);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// ---- VRAM block operations -------------------------------------------------

// A corrupt packet must degrade to a bounded no-op, not a multi-second (or
// unbounded) VRAM stomp - clamp every block op to physical VRAM limits.
static inline int vram_clamp_w(const PS1_RECT* rect, const char* op) {
    int w = rect->w;
    if (w < 0 || w > VRAM_W) {
        WARNF("%s rect suspicious: %d,%d %dx%d", op, rect->x, rect->y,
              rect->w, rect->h);
        w = w < 0 ? 0 : VRAM_W;
    }
    return w;
}
static inline int vram_clamp_h(const PS1_RECT* rect) {
    int h = rect->h;
    return h < 0 ? 0 : (h > VRAM_H ? VRAM_H : h);
}

static void soft_clear_image(const PS1_RECT* rect, u8 r, u8 g, u8 b) {
    clut_cache_flush();
    u16 c = (u16)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
    int w = vram_clamp_w(rect, "clear"), h = vram_clamp_h(rect);
    for (int y = 0; y < h; y++) {
        u16* row = vram + ((rect->y + y) & (VRAM_H - 1)) * VRAM_W;
        for (int x = 0; x < w; x++) {
            row[(rect->x + x) & (VRAM_W - 1)] = c;
        }
    }
}

static void soft_load_image(const PS1_RECT* rect, const u16* src) {
    if (!src) {
        // The PSX's DMA read open bus at address 0 and uploaded garbage
        // without complaint -- MGS's menu does exactly this on purpose
        // (menu_restore_nouse runs before menu_init_nouse fills the panel
        // in). A host dereference of NULL is fatal, so skip the upload; the
        // panel is overwritten with real pixels moments later either way.
        WARNF("load image from NULL (%d,%d %dx%d) skipped", rect->x, rect->y,
              rect->w, rect->h);
        return;
    }
    clut_cache_flush();
    int w = vram_clamp_w(rect, "load"), h = vram_clamp_h(rect);
    (void)w;
    for (int y = 0; y < h; y++) {
        u16* dst = vram + ((rect->y + y) & (VRAM_H - 1)) * VRAM_W + rect->x;
        memcpy(dst, src, (size_t)rect->w * 2);
        src += rect->w;
    }
}

static void soft_store_image(const PS1_RECT* rect, u16* dst) {
    int w = vram_clamp_w(rect, "store"), h = vram_clamp_h(rect);
    (void)w;
    WARNF("store %d,%d %dx%d -> %p", rect->x, rect->y, rect->w, rect->h,
          (void*)dst);
    for (int y = 0; y < h; y++) {
        const u16* src =
            vram + ((rect->y + y) & (VRAM_H - 1)) * VRAM_W + rect->x;
        memcpy(dst, src, (size_t)rect->w * 2);
        dst += rect->w;
    }
}

static void soft_move_image(const PS1_RECT* rect, int dx, int dy) {
    clut_cache_flush();
    int w = vram_clamp_w(rect, "move"), h = vram_clamp_h(rect);
    // clip so both source and destination stay inside VRAM
    int maxx = rect->x > dx ? rect->x : dx;
    int maxy = rect->y > dy ? rect->y : dy;
    if (w > VRAM_W - maxx) w = VRAM_W - maxx;
    if (h > VRAM_H - maxy) h = VRAM_H - maxy;
    if (w <= 0 || h <= 0 || rect->x < 0 || rect->y < 0 || dx < 0 || dy < 0) {
        return;
    }
    // memmove per row, walking in the safe direction for overlap
    if (dy <= rect->y) {
        for (int y = 0; y < h; y++) {
            memmove(vram + (dy + y) * VRAM_W + dx,
                    vram + (rect->y + y) * VRAM_W + rect->x, (size_t)w * 2);
        }
    } else {
        for (int y = h - 1; y >= 0; y--) {
            memmove(vram + (dy + y) * VRAM_W + dx,
                    vram + (rect->y + y) * VRAM_W + rect->x, (size_t)w * 2);
        }
    }
}



// ---- packet parsing into core calls ---------------------------------------
// Mirrors sdl3_gl.c's Draw_PushPrim contract exactly: same s11 decoding, same
// word accounting, same quad split (0,1,2)+(1,3,2).

typedef struct { int x, y; } SoftPos;
static SoftPos display_area_px = {0, 0};
static SoftPos display_size_px = {256, 240};
static int display_enabled = 1;

int Draw_PushPrim(u_long* packets, int max_len) {
    int len = max_len;
    int code = (int)(*packets >> 24) & 0xFF;
    bool isPoly = !(code & 0x40);
    bool isLine = (code & 0x40) && !(code & 0x20);
    bool isTile = (code & 0x40) && (code & 0x20);
    bool isTextured = (code & TEXTURED) != 0;
    bool isGouraud = (code & GOURAUD) != 0;
    bool isShadeTex = !((code & 1) && isTextured && !isLine);
    bool isSemi = (code & SEMITRANSP) != 0;
    u16 tpage = 0, clut = 0;
    SV v[4];

    int r0 = 0x80, g0 = 0x80, b0 = 0x80;
    if (isShadeTex) {
        r0 = (u8)(*packets >> 0);
        g0 = (u8)(*packets >> 8);
        b0 = (u8)(*packets >> 16);
    }
    packets++;
    len--;

    if (isPoly) {
        if (!(code & TRIANGLE)) {
            WARNF("code %02X not supported", code);
            return max_len - len;
        }
        int n = (code & EXTRA_VERTEX) ? 4 : 3;
        for (int i = 0; i < n; i++) {
            if (i == 0 || !isGouraud) {
                v[i].r = r0;
                v[i].g = g0;
                v[i].b = b0;
            }
            if (i > 0 && isGouraud && len > 0) {
                v[i].r = ((u8*)packets)[0];
                v[i].g = ((u8*)packets)[1];
                v[i].b = ((u8*)packets)[2];
                packets++;
                len--;
            }
            if (len <= 0) {
                return max_len - len;
            }
            v[i].x = s11(((short*)packets)[0]);
            v[i].y = s11(((short*)packets)[1]);
            packets++;
            len--;
            if (isTextured && len > 0) {
                v[i].u = ((u8*)packets)[0];
                v[i].v = ((u8*)packets)[1];
                if (i == 0) {
                    clut = ((u16*)packets)[1];
                } else if (i == 1) {
                    tpage = ((u16*)packets)[1];
                }
                packets++;
                len--;
            } else {
                v[i].u = v[i].v = 0;
            }
        }
        if (!isTextured) {
            tpage = cur_tpage;
        }
        if (!isShadeTex) {
            // raw-texture polys: neutral modulation
            for (int i = 0; i < n; i++) {
                v[i].r = v[i].g = v[i].b = 0x80;
            }
        }
        raster_tri(&v[0], &v[1], &v[2], isTextured, tpage, clut, isSemi,
                   isGouraud);
        if (n == 4) {
            raster_tri(&v[1], &v[3], &v[2], isTextured, tpage, clut, isSemi,
                       isGouraud);
        }
    } else if (isLine) {
        bool padding = true;
        int nPoints = ((code >> 2) & 3) + 1;
        if (nPoints == 1) {
            padding = false;
            nPoints++;
        }
        SV pts[4];
        int parsed = 0;
        pts[0].r = r0;
        pts[0].g = g0;
        pts[0].b = b0;
        for (int i = 0; len > 0 && i < nPoints; i++) {
            pts[i].x = s11(((short*)packets)[0]);
            pts[i].y = s11(((short*)packets)[1]);
            packets++;
            len--;
            parsed = i + 1;
            if (len > 0 && i + 1 < nPoints && isGouraud) {
                pts[i + 1].r = ((u8*)packets)[0];
                pts[i + 1].g = ((u8*)packets)[1];
                pts[i + 1].b = ((u8*)packets)[2];
                packets++;
                len--;
            }
        }
        if (!isGouraud) {
            for (int i = 1; i < parsed; i++) {
                pts[i].r = pts[0].r;
                pts[i].g = pts[0].g;
                pts[i].b = pts[0].b;
            }
        }
        for (int i = 0; i + 1 < parsed; i++) {
            raster_line(&pts[i], &pts[i + 1], isSemi);
        }
        if (padding && len > 0) {
            packets++;
            len--;
        }
    } else if (isTile) {
        int w = 0, h = 0;
        v[0].r = r0;
        v[0].g = g0;
        v[0].b = b0;
        v[0].x = s11(((short*)packets)[0]);
        v[0].y = s11(((short*)packets)[1]);
        packets++;
        len--;
        if (isTextured) {
            v[0].u = ((u8*)packets)[0];
            v[0].v = ((u8*)packets)[1];
            clut = ((u16*)packets)[1];
            tpage = cur_tpage;
            packets++;
            len--;
        }
        switch (code & ~3) {
        case 0x60: // TILE
        case 0x64: // SPRT
            w = ((s16*)packets)[0];
            h = ((s16*)packets)[1];
            packets++;
            len--;
            break;
        case 0x68:
        case 0x6C:
            w = h = 1;
            break;
        case 0x70:
        case 0x74:
            w = h = 8;
            break;
        case 0x78:
        case 0x7C:
            w = h = 16;
            break;
        default:
            break;
        }
        if (isTextured) {
            raster_sprite(&v[0], w, h, tpage, clut, isSemi);
        } else {
            raster_tile(&v[0], w, h, isSemi, (cur_tpage >> 5) & 3);
        }
    }
    return max_len - len;
}

// ---- Draw_* state ----------------------------------------------------------

void Draw_SetAreaStart(int x, int y) {
    st.x0 = x;
    st.y0 = y;
}
void Draw_SetAreaEnd(int x, int y) {
    st.x1 = x;
    st.y1 = y;
}
void Draw_SetOffset(int x, int y) {
    x = x % VRAM_W;
    y = y % VRAM_H;
    if (x < 0) {
        x += VRAM_W;
    }
    if (y < 0) {
        y += VRAM_H;
    }
    st.ox = x;
    st.oy = y;
}

void Draw_ClearImage(PS1_RECT* rect, u_char r, u_char g, u_char b) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    soft_clear_image(rect, r, g, b);
}
void Draw_LoadImage(PS1_RECT* rect, u_long* p) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    soft_load_image(rect, (const u16*)p);
}
void Draw_StoreImage(PS1_RECT* rect, u_long* p) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    soft_store_image(rect, (u16*)p);
}
void Draw_MoveImage(PS1_RECT* rect, unsigned int x, unsigned int y) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    soft_move_image(rect, (int)x, (int)y);
}

void Draw_DisplayEnable(unsigned int on) { display_enabled = (int)on; }
void Draw_DisplayArea(unsigned int x, unsigned int y) {
    display_area_px.x = (int)x;
    display_area_px.y = (int)y;
}
void Draw_DisplayHorizontalRange(unsigned int s, unsigned int e) {
    (void)s;
    (void)e;
}
void Draw_DisplayVerticalRange(unsigned int s, unsigned int e) {
    (void)s;
    (void)e;
}
void Draw_SetDisplayMode(DisplayMode* mode) {
    static const int hres[4] = {256, 320, 512, 640};
    display_size_px.x = mode->horizontal_resolution_368
                            ? 368
                            : hres[mode->horizontal_resolution];
    display_size_px.y = mode->vertical_resolution ? 480 : 240;
}
void Draw_PutDispEnv(DISPENV* disp) {
    display_area_px.x = disp->disp.x;
    display_area_px.y = disp->disp.y;
    if (disp->disp.w > 0) {
        display_size_px.x = disp->disp.w;
    }
    if (disp->disp.h > 0) {
        display_size_px.y = disp->disp.h;
    }
}

void Draw_ResetBuffer(void) {}
void Draw_FlushBuffer(void) {}
int Draw_ExequeSync() { return 0; }

