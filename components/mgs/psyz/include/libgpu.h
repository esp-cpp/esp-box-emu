#ifndef LIBGPU_H
#define LIBGPU_H
#include <psyz/types.h>

/**
 * @file libgpu.h
 * @brief GPU (Graphics Processing Unit) Library
 *
 * This library provides low-level control of the PlayStation GPU for 2D
 * graphics rendering, display management, and primitive drawing.
 *
 * Key features:
 * - Drawing primitive setup macros (polygons, lines, sprites)
 * - Texture page and CLUT management
 * - Display environment configuration (resolution, interlace, RGB/NTSC)
 * - Drawing environment setup (drawing area, offset, dither)
 * - VRAM transfer functions (load/store images)
 * - Ordering table (OT) management
 * - Double buffering support
 */

#ifdef __psyz
extern int (*GPU_printf)(const char* fmt, ...);
#else
extern int (*GPU_printf)(); /* printf() object */
#endif

#define limitRange(x, l, h) ((x) = ((x) < (l) ? (l) : (x) > (h) ? (h) : (x)))

#define setVector(v, _x, _y, _z) (v)->vx = (_x), (v)->vy = (_y), (v)->vz = (_z)

#define applyVector(v, _x, _y, _z, op)                                         \
    (v)->vx op _x, (v)->vy op _y, (v)->vz op _z

#define copyVector(v0, v1)                                                     \
    (v0)->vx = (v1)->vx, (v0)->vy = (v1)->vy, (v0)->vz = (v1)->vz

#define addVector(v0, v1)                                                      \
    (v0)->vx += (v1)->vx, (v0)->vy += (v1)->vy, (v0)->vz += (v1)->vz

#define dumpVector(str, v)                                                     \
    GPU_printf("%s=(%d,%d,%d)\n", str, (v)->vx, (v)->vy, (v)->vz)

#define dumpMatrix(x)                                                          \
    GPU_printf("\t%5d,%5d,%5d\n", (x)->m[0][0], (x)->m[0][1], (x)->m[0][2]),   \
        GPU_printf(                                                            \
            "\t%5d,%5d,%5d\n", (x)->m[1][0], (x)->m[1][1], (x)->m[1][2]),      \
        GPU_printf(                                                            \
            "\t%5d,%5d,%5d\n", (x)->m[2][0], (x)->m[2][1], (x)->m[2][2])

#define setRECT(r, _x, _y, _w, _h)                                             \
    (r)->x = (_x), (r)->y = (_y), (r)->w = (_w), (r)->h = (_h)

/*
 *	Set Primitive Attributes
 */
#define setTPage(p, tp, abr, x, y) ((p)->tpage = getTPage(tp, abr, x, y))

#define setClut(p, x, y) ((p)->clut = getClut(x, y))

/*
 * Set Primitive Colors
 */
#define setRGB0(p, _r0, _g0, _b0)                                              \
    (p)->r0 = (_r0), (p)->g0 = (_g0), (p)->b0 = (_b0)

#define setRGB1(p, _r1, _g1, _b1)                                              \
    (p)->r1 = (_r1), (p)->g1 = (_g1), (p)->b1 = (_b1)

#define setRGB2(p, _r2, _g2, _b2)                                              \
    (p)->r2 = (_r2), (p)->g2 = (_g2), (p)->b2 = (_b2)

#define setRGB3(p, _r3, _g3, _b3)                                              \
    (p)->r3 = (_r3), (p)->g3 = (_g3), (p)->b3 = (_b3)

/*
 * Set Primitive Screen Points
 */
#define setXY0(p, _x0, _y0) (p)->x0 = (_x0), (p)->y0 = (_y0)

#define setXY2(p, _x0, _y0, _x1, _y1)                                          \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1)

#define setXY3(p, _x0, _y0, _x1, _y1, _x2, _y2)                                \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1),        \
    (p)->x2 = (_x2), (p)->y2 = (_y2)

#define setXY4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3)                      \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1),        \
    (p)->x2 = (_x2), (p)->y2 = (_y2), (p)->x3 = (_x3), (p)->y3 = (_y3)

#define setXYWH(p, _x0, _y0, _w, _h)                                           \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x0) + (_w), (p)->y1 = (_y0), \
    (p)->x2 = (_x0), (p)->y2 = (_y0) + (_h), (p)->x3 = (_x0) + (_w),           \
    (p)->y3 = (_y0) + (_h)

/*
 * Set Primitive Width/Height
 */
#define setWH(p, _w, _h) (p)->w = (_w), (p)->h = (_h)

/*
 * Set Primitive Texture Points
 */
#define setUV0(p, _u0, _v0) (p)->u0 = (_u0), (p)->v0 = (_v0)

#define setUV3(p, _u0, _v0, _u1, _v1, _u2, _v2)                                \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1),        \
    (p)->u2 = (_u2), (p)->v2 = (_v2)

#define setUV4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3)                      \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1),        \
    (p)->u2 = (_u2), (p)->v2 = (_v2), (p)->u3 = (_u3), (p)->v3 = (_v3)

#define setUVWH(p, _u0, _v0, _w, _h)                                           \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u0) + (_w), (p)->v1 = (_v0), \
    (p)->u2 = (_u0), (p)->v2 = (_v0) + (_h), (p)->u3 = (_u0) + (_w),           \
    (p)->v3 = (_v0) + (_h)

/*
 * Primitive Handling Macros
 */
#define setlen(p, _len) (((P_TAG*)(p))->len = (u_char)(_len))
#define setaddr(p, _addr) (((P_TAG*)(p))->addr = (u_long)(_addr))
#define setcode(p, _code) (((P_TAG*)(p))->code = (u_char)(_code))

#define getlen(p) (u_char)(((P_TAG*)(p))->len)
#define getcode(p) (u_char)(((P_TAG*)(p))->code)
#define getaddr(p) (u_long)(((P_TAG*)(p))->addr)

#ifdef __psyz
#undef nextPrim
#define nextPrim(p) (void*)(((P_TAG*)(p))->addr)
#else
#define nextPrim(p) (void*)((((P_TAG*)(p))->addr) | 0x80000000)
#endif

#define isendprim(p) ((((P_TAG*)(p))->addr) == 0xffffff)

#define addPrim(ot, p) setaddr(p, getaddr(ot)), setaddr(ot, p)
#define addPrims(ot, p0, p1) setaddr(p1, getaddr(ot)), setaddr(ot, p0)

#define catPrim(p0, p1) setaddr(p0, p1)
#define termPrim(p) setaddr(p, 0xffffff)

#define _get_mode(dfe, dtd, tpage)                                             \
    ((0xe1000000) | ((dtd) ? 0x0200 : 0) | ((dfe) ? 0x0400 : 0) |              \
     ((tpage) & 0x9ff))
/* word [2], not [1]: with the 8-byte tag+len header, [1] IS the len field --
 * the packed-layout index left here overwrote the length with the E1 command
 * word and every DR_TPAGE was dropped as a corrupt packet */
#define setDrawTPage(p, dfe, dtd, tpage)                                       \
    setlen(p, 1), ((u_long*)(p))[2] = _get_mode(dfe, dtd, tpage)

#define setPolyF3(p) setlen(p, 4), setcode(p, 0x20)
#define setPolyFT3(p) setlen(p, 7), setcode(p, 0x24)
#define setPolyF4(p) setlen(p, 5), setcode(p, 0x28)
#define setPolyFT4(p) setlen(p, 9), setcode(p, 0x2c)
#define setPolyG3(p) setlen(p, 6), setcode(p, 0x30)
#define setPolyGT3(p) setlen(p, 9), setcode(p, 0x34)
#define setPolyG4(p) setlen(p, 8), setcode(p, 0x38)
#define setPolyGT4(p) setlen(p, 12), setcode(p, 0x3c)
#define setLineF2(p) setlen(p, 3), setcode(p, 0x40)
#define setLineF3(p) setlen(p, 5), setcode(p, 0x48), (p)->pad = 0x55555555
#define setLineF4(p) setlen(p, 6), setcode(p, 0x4c), (p)->pad = 0x55555555
#define setLineG2(p) setlen(p, 4), setcode(p, 0x50)
#define setLineG3(p)                                                           \
    setlen(p, 7), setcode(p, 0x58), (p)->pad = 0x55555555, (p)->p2 = 0
#define setLineG4(p)                                                           \
    setlen(p, 9), setcode(p, 0x5c),                                            \
        (p)->pad = 0x55555555, (p)->p2 = 0, (p)->p3 = 0
#define setTile(p) setlen(p, 3), setcode(p, 0x60)
#define setSprt(p) setlen(p, 4), setcode(p, 0x64)
#define setTile1(p) setlen(p, 2), setcode(p, 0x68)
#define setTile8(p) setlen(p, 2), setcode(p, 0x70)
#define setSprt8(p) setlen(p, 3), setcode(p, 0x74)
#define setTile16(p) setlen(p, 2), setcode(p, 0x78)
#define setSprt16(p) setlen(p, 3), setcode(p, 0x7c)

#define setSemiTrans(p, abe)                                                   \
    ((abe) ? setcode(p, getcode(p) | 0x02) : setcode(p, getcode(p) & ~0x02))

#define setShadeTex(p, tge)                                                    \
    ((tge) ? setcode(p, getcode(p) | 0x01) : setcode(p, getcode(p) & ~0x01))

#define getTPage(tp, abr, x, y)                                                \
    ((((tp) & 0x3) << 7) | (((abr) & 0x3) << 5) | (((y) & 0x100) >> 4) |       \
     (((x) & 0x3ff) >> 6) | (((y) & 0x200) << 2))

#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3f))

typedef struct {
    /* 0x0 */ short x;
    /* 0x2 */ short y; /* offset point on VRAM */
    /* 0x4 */ short w; /* width */
    /* 0x6 */ short h; /* height */
} RECT;                /* size = 0x8 */

/**
 * @brief Rectangular area (32-bit coordinates)
 *
 * Used by library functions to specify a rectangular area of the frame buffer
 * with 32-bit integer coordinates.
 */
typedef struct {
    int x, y; /**< Top left coordinates of the rectangular area */
    int w, h; /**< Width and height of the rectangular area */
} RECT32;

#ifdef __psyz
#define O_TAG                                                                  \
    u_long tag;                                                                \
    u_long len
typedef struct {
    O_TAG;
} OT_TYPE;
#else
#define O_TAG u_long tag
#define OT_TYPE u_long
#endif

typedef struct {
#ifndef __psyz
    unsigned addr : 24;
    unsigned len : 8;
#else
    u_long addr;
    u_long len;
#endif
    u_char r0, g0, b0, code;
} P_TAG;

typedef struct {
    u_char r0, g0, b0, code;
} P_CODE;

typedef struct {
    /* 0x00 */ O_TAG;
    /* 0x4 */ u_long code[15];
} DR_ENV; /* Packed Drawing Environment, size = 0x40 */

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
} POLY_F3; // 0x20

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    short x2, y2;
    u_char u2, v2;
    u_short pad1;
} POLY_FT3; // 0x24

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    short x3, y3;
} POLY_F4; // 0x28

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    short x2, y2;
    u_char u2, v2;
    u_short pad1;
    short x3, y3;
    u_char u3, v3;
    u_short pad2;
} POLY_FT4; // 0x2C

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, pad1;
    short x1, y1;
    u_char r2, g2, b2, pad2;
    short x2, y2;
} POLY_G3; // 0x30

typedef struct {
    O_TAG;
    u_char r0;
    u_char g0;
    u_char b0;
    u_char code;
    short x0;
    short y0;
    u_char u0;
    u_char v0;
    u_short clut;
    u_char r1;
    u_char g1;
    u_char b1;
    u_char p1;
    short x1;
    short y1;
    u_char u1;
    u_char v1;
    u_short tpage;
    u_char r2;
    u_char g2;
    u_char b2;
    u_char p2;
    short x2;
    short y2;
    u_char u2;
    u_char v2;
    u_short pad2;
} POLY_GT3; // 0x34

typedef struct {
    O_TAG;
    u_char r0;
    u_char g0;
    u_char b0;
    u_char code;
    short x0;
    short y0;
    u_char r1;
    u_char g1;
    u_char b1;
    u_char pad1;
    short x1;
    short y1;
    u_char r2;
    u_char g2;
    u_char b2;
    u_char pad2;
    short x2;
    short y2;
    u_char r3;
    u_char g3;
    u_char b3;
    u_char pad3;
    short x3;
    short y3;
} POLY_G4; // 0x38

typedef struct {
    O_TAG;
    u_char r0;
    u_char g0;
    u_char b0;
    u_char code;
    short x0;
    short y0;
    u_char u0;
    u_char v0;
    u_short clut;
    u_char r1;
    u_char g1;
    u_char b1;
    u_char p1;
    short x1;
    short y1;
    u_char u1;
    u_char v1;
    u_short tpage;
    u_char r2;
    u_char g2;
    u_char b2;
    u_char p2;
    short x2;
    short y2;
    u_char u2;
    u_char v2;
    u_short pad2;
    u_char r3;
    u_char g3;
    u_char b3;
    u_char p3;
    short x3;
    short y3;
    u_char u3;
    u_char v3;
    u_short pad3;
} POLY_GT4; // 0x3C

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
} LINE_F2; // 0x40

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    u_long pad;
} LINE_F3; // 0x48

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    short x3, y3;
    u_long pad;
} LINE_F4; // 0x4C

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, p1;
    short x1, y1;
} LINE_G2; // 0x50

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, p1;
    short x1, y1;
    u_char r2, g2, b2, p2;
    short x2, y2;
    u_long pad;
} LINE_G3; // 0x58

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, p1;
    short x1, y1;
    u_char r2, g2, b2, p2;
    short x2, y2;
    u_char r3, g3, b3, p3;
    short x3, y3;
    u_long pad;
} LINE_G4; // 0x5C

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    short w, h;
} TILE; // 0x60

typedef struct {
    O_TAG;
    u_char r0;
    u_char g0;
    u_char b0;
    u_char code;
    short x0;
    short y0;
    u_char u0;
    u_char v0;
    u_short clut;
    short w;
    short h;
} SPRT; // 0x64

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
} TILE_1; // 0x68

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
} TILE_8; // 0x70

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
} SPRT_8; // 0x74

typedef struct {
    O_TAG;
    u_char r0, g0, b0, code;
    short x0, y0;
} TILE_16; // 0x78

typedef struct {
    O_TAG;
    u_char r0;
    u_char g0;
    u_char b0;
    u_char code;
    short x0;
    short y0;
    u_char u0;
    u_char v0;
    u_short clut;
} SPRT_16; // 0x7C

/*
 *  Special Primitive Definitions
 */
typedef struct {
    O_TAG;
    u_long code[2];
} DR_MODE; /* Drawing Mode */

typedef struct {
    O_TAG;
    u_long code[2];
} DR_TWIN; /* Texture Window */

typedef struct {
    O_TAG;
    u_long code[2];
} DR_AREA; /* Drawing Area */

typedef struct {
    O_TAG;
    u_long code[2];
} DR_OFFSET; /* Drawing Offset */

typedef struct {
    O_TAG;
    u_long code[2];
} DR_STP; /* STP bit update */

typedef struct { /* MoveImage */
    O_TAG;
    u_long code[5];
} DR_MOVE;

typedef struct { /* LoadImage */
    O_TAG;
    u_long code[3];
    u_long p[13];
} DR_LOAD;

typedef struct {
    O_TAG;
    u_long code[1];
} DR_TPAGE; /* Drawing TPage */

/*
 *	Font Stream Parameters
 */
#define FNT_MAX_ID 8      /* max number of stream ID */
#define FNT_MAX_SPRT 1024 /* max number of sprites in all streams */

typedef struct {
    /* 0x00 */ RECT clip;     /* clip area */
    /* 0x08 */ short ofs[2];  /* drawing offset */
    /* 0x0C */ RECT tw;       /* texture window */
    /* 0x14 */ u_short tpage; /* texture page */
    /* 0x16 */ u_char dtd;    /* dither flag (0:off, 1:on) */
    /* 0x17 */ u_char dfe;    /* flag to draw on display area (0:off 1:on) */
    /* 0x18 */ u_char isbg;   /* enable to auto-clear */
    /* 0x19 */ u_char r0, g0, b0; /* initital background color */
    /* 0x1C */ DR_ENV dr_env;     /* reserved */
} DRAWENV;                        /* size = 0x58 */

typedef struct {
    /* 0x00 */ RECT disp;      /* display area */
    /* 0x08 */ RECT screen;    /* display start point */
    /* 0x10 */ u_char isinter; /* interlace 0: off 1: on */
    /* 0x11 */ u_char isrgb24; /* RGB24 bit mode */
    /* 0x12 */ u_char pad0;    /* reserved */
    /* 0x13 */ u_char pad1;    /* reserved */
} DISPENV;                     /* size = 0x14 */

typedef struct PixPattern {
    u8 w;
    u8 h;
    u8 x;
    u8 y;
} PixPattern;

/*
 *	Multi-purpose TIM image
 */
typedef struct {
    u_long mode;   /* pixel mode */
    RECT* crect;   /* CLUT rectangle on frame buffer */
    u_long* caddr; /* CLUT address on main memory */
    RECT* prect;   /* texture image rectangle on frame buffer */
    u_long* paddr; /* texture image address on main memory */
} TIM_IMAGE;

/**
 * @brief Load texture pattern to frame buffer
 *
 * Loads a texture pattern from the memory area starting at the address pix into
 * the frame buffer area starting at the address (x, y), and calculates the
 * texture page ID for the loaded texture pattern. The texture pattern size w
 * represents the number of pixels, not the actual size of the transfer area in
 * the frame buffer.
 *
 * @param pix Pointer to texture pattern start address
 * @param tp Bit depth (0 = 4-bit; 1 = 8-bit; 2 = 16-bit)
 * @param abr Semitransparency rate
 * @param x Destination frame buffer X address
 * @param y Destination frame buffer Y address
 * @param w Texture pattern width
 * @param h Texture pattern height
 * @return Texture page ID
 */
extern u_short LoadTPage(
    u_long* pix, int tp, int abr, int x, int y, int w, int h);

/**
 * @brief Load CLUT to frame buffer
 *
 * @param clut Pointer to CLUT data
 * @param x Horizontal frame buffer address
 * @param y Vertical frame buffer address
 * @return CLUT ID
 */
extern u_short LoadClut(u_long* clut, int x, int y);

/**
 * @brief Load CLUT to frame buffer (alternative)
 *
 * @param clut Pointer to CLUT data
 * @param x Horizontal frame buffer address
 * @param y Vertical frame buffer address
 * @return CLUT ID
 */
extern u_short LoadClut2(u_long* clut, int x, int y);

/**
 * @brief Calculate and return texture CLUT ID
 *
 * The CLUT address is limited to multiples of 16 in the x direction.
 *
 * @param x Horizontal frame buffer address of CLUT
 * @param y Vertical frame buffer address of CLUT
 * @return CLUT ID
 */
u_short GetClut(int x, int y);

/**
 * @brief Calculate and return texture page ID
 *
 * @param tp Texture mode (0=4bit, 1=8bit, 2=16bit)
 * @param abr Semi-transparency rate (0=0.5, 1=1.0, 2=1.0, 3=0.25)
 * @param x Texture page X position in frame buffer
 * @param y Texture page Y position in frame buffer
 * @return Texture page ID
 */
u_short GetTPage(int tp, int abr, int x, int y);

/**
 * @brief Get next primitive in list
 *
 * @param p Pointer to current primitive
 * @return Pointer to next primitive
 */
extern void* NextPrim(void* p);

/**
 * @brief Register a primitive to the OT
 *
 * Registers a primitive beginning with the address *p to the OT entry *ot in
 * OT table. A primitive may be added to a primitive list only once in the same
 * frame.
 *
 * @param ot OT entry
 * @param p Start address of primitive to be registered
 */
extern void AddPrim(void* ot, void* p);

/**
 * @brief Collectively register primitives to the OT
 *
 * Registers primitives beginning with p0 and ending with p1 to the *ot entry
 * in the OT.
 *
 * @param ot OT entry
 * @param p0 Start address of primitive list
 * @param p1 End address of primitive list
 */
extern void AddPrims(void* ot, void* p0, void* p1);

/**
 * @brief Concatenate primitives
 *
 * @param p0 First primitive
 * @param p1 Second primitive
 */
extern void CatPrim(void* p0, void* p1);

/**
 * @brief Draw ordering table
 *
 * @param p Pointer to ordering table
 */
extern void DrawOTag(OT_TYPE* p);

/**
 * @brief Draw ordering table with I/O
 *
 * @param p Pointer to ordering table
 */
extern void DrawOTagIO(OT_TYPE* p);

/**
 * @brief Draw ordering table with environment
 *
 * @param p Pointer to ordering table
 * @param env Drawing environment
 */
extern void DrawOTagEnv(OT_TYPE* p, DRAWENV* env);

/**
 * @brief Draw primitive
 *
 * @param p Pointer to primitive
 */
extern void DrawPrim(void* p);

/**
 * @brief Dump CLUT information
 *
 * @param clut CLUT ID
 */
extern void DumpClut(u_short clut);

/**
 * @brief Dump display environment
 *
 * @param env Display environment
 */
extern void DumpDispEnv(DISPENV* env);

/**
 * @brief Dump drawing environment
 *
 * @param env Drawing environment
 */
extern void DumpDrawEnv(DRAWENV* env);

/**
 * @brief Dump ordering table
 *
 * @param p Pointer to ordering table
 */
extern void DumpOTag(OT_TYPE* p);

/**
 * @brief Dump texture page information
 *
 * @param tpage Texture page ID
 */
extern void DumpTPage(u_short tpage);

/**
 * @brief Load font pattern
 *
 * @param tx X position in frame buffer
 * @param ty Y position in frame buffer
 */
extern void FntLoad(int tx, int ty);

/**
 * @brief Set display mask
 *
 * @param mask 0: display off, 1: display on
 */
extern void SetDispMask(int mask);

/**
 * @brief Set drawing area primitive
 *
 * @param p Drawing area primitive
 * @param r Rectangle area
 */
extern void SetDrawArea(DR_AREA* p, RECT* r);

/**
 * @brief Set drawing environment primitive
 *
 * @param dr_env Drawing environment primitive
 * @param env Drawing environment
 */
extern void SetDrawEnv(DR_ENV* dr_env, DRAWENV* env);

/**
 * @brief Set load image primitive
 *
 * @param p Load image primitive
 * @param rect Rectangle area
 */
extern void SetDrawLoad(DR_LOAD* p, RECT* rect);

/**
 * @brief Set drawing mode primitive
 *
 * @param p Drawing mode primitive
 * @param dfe Drawing to display area flag
 * @param dtd Dithering flag
 * @param tpage Texture page
 * @param tw Texture window
 */
extern void SetDrawMode(DR_MODE* p, int dfe, int dtd, int tpage, RECT* tw);

/**
 * @brief Set texture page primitive
 *
 * @param p Texture page primitive
 * @param dfe Drawing to display area flag
 * @param dtd Dithering flag
 * @param tpage Texture page ID
 */
extern void SetDrawTPage(DR_TPAGE* p, int dfe, int dtd, int tpage);

/**
 * @brief Set move image primitive
 *
 * @param p Move image primitive
 * @param rect Source rectangle
 * @param x Destination X coordinate
 * @param y Destination Y coordinate
 */
extern void SetDrawMove(DR_MOVE* p, RECT* rect, int x, int y);

/**
 * @brief Set drawing offset primitive
 *
 * @param p Drawing offset primitive
 * @param ofs Offset values [X, Y]
 */
extern void SetDrawOffset(DR_OFFSET* p, u_short* ofs);

/**
 * @brief Set debug font stream ID
 *
 * @param id Stream ID
 */
extern void SetDumpFnt(int id);

/**
 * @brief Initialize flat-shaded line primitive (2 vertices)
 *
 * @param p Line primitive
 */
extern void SetLineF2(LINE_F2* p);

/**
 * @brief Initialize flat-shaded line primitive (3 vertices)
 *
 * @param p Line primitive
 */
extern void SetLineF3(LINE_F3* p);

/**
 * @brief Initialize flat-shaded line primitive (4 vertices)
 *
 * @param p Line primitive
 */
extern void SetLineF4(LINE_F4* p);

/**
 * @brief Initialize Gouraud-shaded line primitive (2 vertices)
 *
 * @param p Line primitive
 */
extern void SetLineG2(LINE_G2* p);

/**
 * @brief Initialize Gouraud-shaded line primitive (3 vertices)
 *
 * @param p Line primitive
 */
extern void SetLineG3(LINE_G3* p);

/**
 * @brief Initialize Gouraud-shaded line primitive (4 vertices)
 *
 * @param p Line primitive
 */
extern void SetLineG4(LINE_G4* p);

/**
 * @brief Initialize flat-shaded triangle primitive
 *
 * @param p Polygon primitive
 */
extern void SetPolyF3(POLY_F3* p);

/**
 * @brief Initialize flat-shaded quadrangle primitive
 *
 * @param p Polygon primitive
 */
extern void SetPolyF4(POLY_F4* p);

/**
 * @brief Initialize flat-shaded, texture-mapped triangle primitive
 *
 * @param p Polygon primitive
 */
extern void SetPolyFT3(POLY_FT3* p);

/**
 * @brief Initialize flat-shaded, texture-mapped quadrangle primitive
 *
 * @param p Polygon primitive
 */
extern void SetPolyFT4(POLY_FT4* p);

/**
 * @brief Initialize Gouraud-shaded triangle primitive
 *
 * @param p Polygon primitive
 */
extern void SetPolyG3(POLY_G3* p);

/**
 * @brief Initialize Gouraud-shaded quadrangle primitive
 *
 * @param p Polygon primitive
 */
extern void SetPolyG4(POLY_G4* p);

/**
 * @brief Initialize Gouraud-shaded, texture-mapped triangle primitive
 *
 * @param p Polygon primitive
 */
extern void SetPolyGT3(POLY_GT3* p);

/**
 * @brief Initialize Gouraud-shaded, texture-mapped quadrangle primitive
 *
 * @param p Polygon primitive
 */
extern void SetPolyGT4(POLY_GT4* p);

/**
 * @brief Set semi-transparency attribute
 *
 * @param p Primitive
 * @param abe Semi-transparency flag (0: off, 1: on)
 */
extern void SetSemiTrans(void* p, int abe);

/**
 * @brief Set texture shading attribute
 *
 * @param p Primitive
 * @param tge Texture shading flag (0: texture off, 1: texture and shade on)
 */
extern void SetShadeTex(void* p, int tge);

/**
 * @brief Initialize sprite primitive
 *
 * @param p Sprite primitive
 */
extern void SetSprt(SPRT* p);

/**
 * @brief Initialize 16x16 sprite primitive
 *
 * @param p Sprite primitive
 */
extern void SetSprt16(SPRT_16* p);

/**
 * @brief Initialize 8x8 sprite primitive
 *
 * @param p Sprite primitive
 */
extern void SetSprt8(SPRT_8* p);

/**
 * @brief Set texture window primitive
 *
 * @param p Texture window primitive
 * @param tw Texture window rectangle
 */
extern void SetTexWindow(DR_TWIN* p, RECT* tw);

/**
 * @brief Initialize tile primitive
 *
 * @param p Tile primitive
 */
extern void SetTile(TILE* p);

/**
 * @brief Initialize 1x1 tile primitive
 *
 * @param p Tile primitive
 */
extern void SetTile1(TILE_1* p);

/**
 * @brief Initialize 16x16 tile primitive
 *
 * @param p Tile primitive
 */
extern void SetTile16(TILE_16* p);

/**
 * @brief Initialize 8x8 tile primitive
 *
 * @param p Tile primitive
 */
extern void SetTile8(TILE_8* p);

/**
 * @brief Terminate primitive list
 *
 * @param p Primitive
 */
extern void TermPrim(void* p);

/**
 * @brief Reset graphics system
 *
 * @param mode Reset mode (0: complete reset, 1: cancels only reset of drawing
 * engine, 3: reset without video mode change)
 * @return Previous video mode
 */
extern int ResetGraph(int mode);

/**
 * @brief Set graphics debug level
 *
 * @param level Debug level
 * @return Previous debug level
 */
int SetGraphDebug(int level);

/**
 * @brief Set graphics reverse mode
 *
 * @param mode Reverse mode
 * @return Previous mode
 */
extern int SetGraphReverse(int mode);

/**
 * @brief Set graphics queue mode
 *
 * @param mode Queue mode
 * @return Previous mode
 */
extern int SetGraphQueue(int mode);

/**
 * @brief Set drawing completion callback
 *
 * @param func Callback function
 * @return Previous callback function
 */
extern u_long DrawSyncCallback(void (*func)());

/**
 * @brief Print formatted text to debug font stream
 *
 * @param fmt Format string
 * @return Number of characters printed
 */
int FntPrint(const char* fmt, ...);

/**
 * @brief Check primitive validity
 *
 * @param s Debug message string
 * @param p Primitive
 * @return 1 if valid, 0 if invalid
 */
extern int CheckPrim(char* s, OT_TYPE* p);

/**
 * @brief Clear frame buffer rectangle
 *
 * @param rect Rectangle area
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @return 1 on success
 */
extern int ClearImage(RECT* rect, u_char r, u_char g, u_char b);

/**
 * @brief Wait for drawing to finish
 *
 * @param mode 0: wait for completion, 1: return immediately
 * @return 0 if drawing complete, positive if drawing in progress
 */
extern int DrawSync(int mode);

/**
 * @brief Open debug font stream
 *
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param isbg Background clear flag
 * @param n Maximum characters
 * @return Stream ID, or -1 on error
 */
extern int FntOpen(int x, int y, int w, int h, int isbg, int n);

/**
 * @brief Get GPU hardware revision
 *
 * @return can be either 0, 1 or 2
 */
extern int GetGraphType(void);

/**
 * @brief Get graphics debug level
 *
 * @return Current debug level
 */
extern int GetGraphDebug(void);

/**
 * @brief Flush debug font stream
 *
 * @param id Stream ID
 * @return Pointer to primitive, or NULL if buffer empty
 */
extern u_long* FntFlush(int id);

/**
 * @brief Open kanji font stream
 *
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param dx Character width
 * @param dy Character height
 * @param cx Columns
 * @param cy Rows
 * @param isbg Background clear flag
 * @param n Maximum characters
 * @return Stream ID, or -1 on error
 */
extern int KanjiFntOpen(int x, int y, int w, int h, int dx, int dy, int cx,
                        int cy, int isbg, int n);

/**
 * @brief Load image from memory to frame buffer
 *
 * @param rect Destination rectangle in frame buffer
 * @param p Pointer to image data
 * @return 1 on success
 */
extern int LoadImage(RECT* rect, u_long* p);

/**
 * @brief Merge primitive lists
 *
 * @param p0 First primitive list
 * @param p1 Second primitive list
 * @return 1 on success
 */
extern int MargePrim(void* p0, void* p1);

/**
 * @brief Store image from frame buffer to memory
 *
 * @param rect Source rectangle in frame buffer
 * @param p Pointer to destination buffer
 * @return 1 on success
 */
extern int StoreImage(RECT* rect, u_long* p);

/**
 * @brief Move image within frame buffer
 *
 * @param rect Source rectangle
 * @param x Destination X coordinate
 * @param y Destination Y coordinate
 * @return 1 on success
 */
extern int MoveImage(RECT* rect, int x, int y);

/**
 * @brief Open TIM image file
 *
 * @param addr Pointer to TIM data
 * @return 0 on success, -1 on error
 */
extern int OpenTIM(u_long* addr);

/**
 * @brief Clear ordering table
 *
 * @param ot Pointer to ordering table
 * @param n Number of entries
 * @return Pointer to ordering table
 */
extern OT_TYPE* ClearOTag(OT_TYPE* ot, int n);

/**
 * @brief Clear ordering table in reverse
 *
 * @param ot Pointer to ordering table
 * @param n Number of entries
 * @return Pointer to ordering table
 */
extern OT_TYPE* ClearOTagR(OT_TYPE* ot, int n);

/**
 * @brief Set drawing environment
 *
 * @param env Drawing environment
 * @return Pointer to drawing environment
 */
extern DRAWENV* PutDrawEnv(DRAWENV* env);

/**
 * @brief Set display environment
 *
 * @param env Display environment
 * @return Pointer to display environment
 */
extern DISPENV* PutDispEnv(DISPENV* env);

/**
 * @brief Set default display environment
 *
 * @param env Display environment
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @return Pointer to display environment
 */
extern DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int w, int h);

/**
 * @brief Set default drawing environment
 *
 * @param env Drawing environment
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @return Pointer to drawing environment
 */
extern DRAWENV* SetDefDrawEnv(DRAWENV* env, int x, int y, int w, int h);

/**
 * @brief Read TIM image
 *
 * @param timimg TIM image structure
 * @return Pointer to TIM image structure
 */
extern TIM_IMAGE* ReadTIM(TIM_IMAGE* timimg);

/**
 * @brief Interrupt drawing
 *
 * Interrupts drawing after the current polygon is drawn. The return value is
 * the next drawing entry; to resume drawing, pass this value to DrawOTag().
 *
 * @return Next polygon drawing entry (0xffffffff during DMA transfer)
 */
extern u_long* BreakDraw(void);

/**
 * @brief Continue drawing interrupted OT
 *
 * Continue to draw the OT interrupted by BreakDraw(). Immediately executes the
 * OT supplied by inst_ot without entering it in the libgpu queue.
 *
 * @param inst_ot OT to execute immediately
 * @param cont_ot OT to draw after inst_ot completes
 */
extern void ContinueDraw(u_long* inst_ot, u_long* cont_ot);

/**
 * @brief Draw ordering table without queueing
 *
 * Immediately executes an OT without queueing. When drawing is suspended with
 * BreakDraw() after DrawOTag2() is called, confirm completion of data transfer
 * using IsIdleGPU() before restarting with ContinueDraw().
 *
 * @param p Pointer to ordering table
 */
extern void DrawOTag2(u_long* p);

/**
 * @brief Check if drawing suspended by BreakDraw() was completed
 *
 * When drawing is suspended by BreakDraw(), the GPU doesn't stop until drawing
 * of the current primitive is completed. This function checks whether the
 * drawing suspended by BreakDraw() has completed.
 *
 * @param maxcount Number of times to check for idle before returning
 * @return 0 if GPU is idle, 1 if still busy
 */
extern int IsIdleGPU(int maxcount);

/**
 * @brief Get display environment
 *
 * @param env Display environment to receive current settings
 * @return Pointer to display environment
 */
extern DISPENV* GetDispEnv(DISPENV* env);

/**
 * @brief Get drawing environment
 *
 * @param env Drawing environment to receive current settings
 * @return Pointer to drawing environment
 */
extern DRAWENV* GetDrawEnv(DRAWENV* env);

/**
 * @brief Get drawing environment (alternative)
 *
 * @param env Drawing environment to receive current settings
 * @return Pointer to drawing environment
 */
extern DRAWENV* GetDrawEnv2(DRAWENV* env);

/**
 * @brief Get current drawing area
 *
 * @param area Rectangle to receive current drawing area
 * @return Pointer to rectangle
 */
extern RECT* GetDrawArea(RECT* area);

/**
 * @brief Get current drawing mode
 *
 * @param dfe Pointer to receive drawing to display area flag
 * @param dtd Pointer to receive dithering flag
 * @param tpage Pointer to receive texture page
 * @param tw Pointer to receive texture window
 */
extern void GetDrawMode(int* dfe, int* dtd, int* tpage, RECT* tw);

/**
 * @brief Get current drawing offset
 *
 * @param ofs Array to receive offset values [X, Y]
 */
extern void GetDrawOffset(u_short* ofs);

/**
 * @brief Get texture window settings
 *
 * @param tw Rectangle to receive texture window settings
 * @return Pointer to rectangle
 */
extern RECT* GetTexWindow(RECT* tw);

/**
 * @brief Get ordering table draw enable flag
 *
 * @return ODE flag value
 */
extern int GetODE(void);

/**
 * @brief Get TIM image size
 *
 * @param addr Pointer to TIM data
 * @return Size of TIM image in bytes
 */
extern int GetTimSize(u_long* addr);

/**
 * @brief Clear frame buffer rectangle (alternative)
 *
 * @param rect Rectangle area
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @return 1 on success
 */
extern int ClearImage2(RECT* rect, u_char r, u_char g, u_char b);

/**
 * @brief Load image from memory to frame buffer (non-blocking)
 *
 * Non-blocking version of LoadImage(). Use IsIdleGPU() to check completion.
 *
 * @param rect Destination rectangle in frame buffer
 * @param p Pointer to image data
 * @return 1 on success
 */
extern int LoadImage2(RECT* rect, u_long* p);

/**
 * @brief Store image from frame buffer to memory (non-blocking)
 *
 * Non-blocking version of StoreImage(). Use IsIdleGPU() to check completion.
 *
 * @param rect Source rectangle in frame buffer
 * @param p Pointer to destination buffer
 * @return 1 on success
 */
extern int StoreImage2(RECT* rect, u_long* p);

/**
 * @brief Move image within frame buffer (non-blocking)
 *
 * Non-blocking version of MoveImage(). Use IsIdleGPU() to check completion.
 *
 * @param rect Source rectangle
 * @param x Destination X coordinate
 * @param y Destination Y coordinate
 * @return 1 on success
 */
extern int MoveImage2(RECT* rect, int x, int y);

/**
 * @brief Set STP bit primitive
 *
 * @param p STP primitive
 * @param stp STP bit value
 */
extern void SetDrawStp(DR_STP* p, int stp);

/**
 * @brief Close kanji font stream
 *
 * @param id Stream ID
 */
extern void KanjiFntClose(int id);

/**
 * @brief Flush kanji font stream
 *
 * @param id Stream ID
 * @return Pointer to primitive, or NULL if buffer empty
 */
extern u_long* KanjiFntFlush(int id);

/**
 * @brief Print formatted kanji text to font stream
 *
 * @param id Stream ID
 * @param fmt Format string
 * @return Number of characters printed
 */
extern int KanjiFntPrint(int id, const char* fmt, ...);

/**
 * @brief Convert KROM font to TIM format
 *
 * @param sjis Shift-JIS character code
 * @param tim Pointer to TIM image structure
 * @return Pointer to TIM image structure
 */
extern TIM_IMAGE* Krom2Tim(u_short sjis, TIM_IMAGE* tim);

/**
 * @brief Open TMD file
 *
 * @param addr Pointer to TMD data
 * @param obj_no Object number
 * @return The number of polygons comprising the object as a positive integer;
 * on failure, returns 0.
 */
extern int OpenTMD(u_long* addr, int obj_no);

/**
 * @brief Read TMD file
 *
 * @param addr Pointer to TMD data
 * @return tmdprim if successful; 0 on failure.
 */
extern int ReadTMD(u_long* addr);

#endif
