#include <assert.h>
#include <psyz.h>
#include <psyz/log.h>
#include <libgpu.h>
#include "../internal.h"

// This GTE implementation is mostly accurate to how the PS1 computes math.
// Most of the implementation needs 64-bit vars for accuracy, which can be slow
// on 32-bit hardware where the type `long long` is software emulated.
//
// There are yet no target-specific code paths here. Most consoles such as PS2,
// Dreamcast or GBA could use different code paths to use hardware accelerated
// math, while ensuring a decent level of accuracy.
//
// https://github.com/nicolasnoble/pcsx-redux/tree/main/src/mips/tests/gte
// The above test suite from Nicolas Noble, one of the main PCSX Redux emulator
// developers, has been used to verify this GTE emulation is accurate enough.

// https://www.problemkaputt.de/psx-spx.htm#gteoverview
static SVECTOR V0;         // cop1 0-1
static SVECTOR V1;         // cop1 2-3
static SVECTOR V2;         // cop1 4-5
static CVECTOR RGBC;       // cop1 6
static unsigned short OTZ; // cop1 7 average Z value
static short IR0;          // cop1 8 accumulator, interpolate
static short IR1;          // cop1 9 accumulator, vector x
static short IR2;          // cop1 10 accumulator, vector y
static short IR3;          // cop1 11 accumulator, vector z
static short SX0, SY0;     // cop1 12
static short SX1, SY1;     // cop1 13
static short SX2, SY2;     // cop1 14
static short SXP, SYP;     // cop1 15
static unsigned short SZ0; // cop1 16 screen Z-coordinate FIFO
static unsigned short SZ1; // cop1 17 screen Z-coordinate FIFO
static unsigned short SZ2; // cop1 18 screen Z-coordinate FIFO
static unsigned short SZ3; // cop1 19 screen Z-coordinate FIFO
static int MAC0;           // cop1 24 math accumulator (value)
static int MAC1;           // cop1 25 math accumulator (vector)
static int MAC2;           // cop1 26 math accumulator (vector)
static int MAC3;           // cop1 27 math accumulator (vector)
static unsigned int RGB0;  // cop1 20 color FIFO
static unsigned int RGB1;  // cop1 21
static unsigned int RGB2;  // cop1 22
static unsigned int RES1;  // cop1 23 (reserved)
static MATRIX M = {0};     // cop2 0-7, rotation 3x3 + translation
static MATRIX L1 = {0};    // cop2 8-15 light source 3x3 + bg color
static MATRIX L2 = {0};    // cop2 16-23 light source 3x3 + bg color
static int OFX;            // cop2 24 screen offset X
static int OFY;            // cop2 25 screen offset Y
static unsigned short H;   // cop2 26 projection plane distance
static short DQA;          // cop2 27 depth queing parameter A (coeff)
static int DQB;            // cop2 28 depth queing parameter B (offset, s32)
static short ZSF3;         // cop2 29 average Z scale factor
static short ZSF4;         // cop2 30 average Z scale factor
static int LZCS;           // cop1 30 leading-zero count source
static int LZCR;           // cop1 31 leading-zero count result
static unsigned int FLAG;  // cop2 31

static unsigned int pack_xy(short x, short y);
static void MVMVA(unsigned int cmd25);

// FLAG register bits
#define FLAG_MAC1_OVF_POS (1u << 30) // MAC1 overflow > +(1<<43)-1
#define FLAG_MAC2_OVF_POS (1u << 29)
#define FLAG_MAC3_OVF_POS (1u << 28)
#define FLAG_MAC1_OVF_NEG (1u << 27) // MAC1 overflow < -(1<<43)
#define FLAG_MAC2_OVF_NEG (1u << 26)
#define FLAG_MAC3_OVF_NEG (1u << 25)
#define FLAG_IR1_SAT (1u << 24)
#define FLAG_IR2_SAT (1u << 23)
#define FLAG_IR3_SAT (1u << 22)
#define FLAG_COL_R_SAT (1u << 21) // RGB FIFO R saturated to 0..ff
#define FLAG_COL_G_SAT (1u << 20)
#define FLAG_COL_B_SAT (1u << 19)
#define FLAG_SZ3_OTZ_SAT (1u << 18)  // SZ3 or OTZ saturated to 0..ffff
#define FLAG_DIV_OVF (1u << 17)      // Divide overflow
#define FLAG_MAC0_OVF_POS (1u << 16) // MAC0 overflow > +(1<<31)-1
#define FLAG_MAC0_OVF_NEG (1u << 15) // MAC0 overflow < -(1<<31)
#define FLAG_SX2_SAT (1u << 14)
#define FLAG_SY2_SAT (1u << 13)
#define FLAG_IR0_SAT (1u << 12)
#define FLAG_ERROR_MASK 0x7F87E000u
#define FLAG_ERROR (1u << 31)

// Update bit 31 based on error bits
static void FLAG_update_error() {
    if (FLAG & FLAG_ERROR_MASK) {
        FLAG |= FLAG_ERROR;
    }
}

void InitGeom() {
    ZSF3 = 0x155;
    ZSF4 = 0x100;
    H = 1000;
    DQA = -0x1062;
    DQB = 0x140;
    OFX = 0;
    OFY = 0;
}

void SetGeomOffset(long ofx, long ofy) {
    OFX = (short)(ofx & 0xFFFF);
    OFY = (short)(ofy & 0xFFFF);
}

void SetGeomScreen(long h) { H = h; }

void SetRotMatrix(MATRIX* m) {
    M.m[0][0] = m->m[0][0];
    M.m[0][1] = m->m[0][1];
    M.m[0][2] = m->m[0][2];
    M.m[1][0] = m->m[1][0];
    M.m[1][1] = m->m[1][1];
    M.m[1][2] = m->m[1][2];
    M.m[2][0] = m->m[2][0];
    M.m[2][1] = m->m[2][1];
    M.m[2][2] = m->m[2][2];
}

void SetTransMatrix(MATRIX* m) {
    M.t[0] = m->t[0];
    M.t[1] = m->t[1];
    M.t[2] = m->t[2];
}

void SetTransVector(VECTOR* v) {
    M.t[0] = v->vx;
    M.t[1] = v->vy;
    M.t[2] = v->vz;
}

void SetLightMatrix(MATRIX* m) {
    L1.m[0][0] = m->m[0][0];
    L1.m[0][1] = m->m[0][1];
    L1.m[0][2] = m->m[0][2];
    L1.m[1][0] = m->m[1][0];
    L1.m[1][1] = m->m[1][1];
    L1.m[1][2] = m->m[1][2];
    L1.m[2][0] = m->m[2][0];
    L1.m[2][1] = m->m[2][1];
    L1.m[2][2] = m->m[2][2];
}

void SetBackColor(long rbk, long gbk, long bbk) {
    L1.t[0] = (int)(rbk << 4);
    L1.t[1] = (int)(gbk << 4);
    L1.t[2] = (int)(bbk << 4);
}

void SetColorMatrix(MATRIX* m) {
    L2.m[0][0] = m->m[0][0];
    L2.m[0][1] = m->m[0][1];
    L2.m[0][2] = m->m[0][2];
    L2.m[1][0] = m->m[1][0];
    L2.m[1][1] = m->m[1][1];
    L2.m[1][2] = m->m[1][2];
    L2.m[2][0] = m->m[2][0];
    L2.m[2][1] = m->m[2][1];
    L2.m[2][2] = m->m[2][2];
}

void SetFarColor(long rfc, long gfc, long bfc) {
    L2.t[0] = (int)(rfc << 4);
    L2.t[1] = (int)(gfc << 4);
    L2.t[2] = (int)(bfc << 4);
}

void SetFogNear(long a, long h) { NOT_IMPLEMENTED; }

void Psyz_GteLdRgb(CVECTOR* v) { *(unsigned int*)&RGBC = *(unsigned int*)v; }
void Psyz_GteStRgb(CVECTOR* v) { *(unsigned int*)v = RGB2; }

void Psyz_GteLdClmv(void* p) {
    short* s = (short*)p;
    IR1 = s[0];
    IR2 = s[3];
    IR3 = s[6];
}

void Psyz_GteStClmv(void* p) {
    short* s = (short*)p;
    s[0] = IR1;
    s[3] = IR2;
    s[6] = IR3;
}

void Psyz_GteLdTr(long tx, long ty, long tz) {
    M.t[0] = (int)tx;
    M.t[1] = (int)ty;
    M.t[2] = (int)tz;
}

void Psyz_GteLdTx(long v) { M.t[0] = (int)v; }
void Psyz_GteLdTy(long v) { M.t[1] = (int)v; }
void Psyz_GteLdTz(long v) { M.t[2] = (int)v; }

long SquareRoot0_impl(long a);
long SquareRoot0(long a) { return SquareRoot0_impl(a); }

long SquareRoot12_impl(long a);
long SquareRoot12(long a) { return SquareRoot12_impl(a); }

MATRIX* RotMatrix(SVECTOR* r, MATRIX* m) {
#ifdef PLATFORM_64BIT
    // 64-bit version, accurate with PS1 implementation
    long long cx = rcos(r->vx);
    long long sx = rsin(r->vx);
    long long cy = rcos(r->vy);
    long long sy = rsin(r->vy);
    long long cz = rcos(r->vz);
    long long sz = rsin(r->vz);

    m->m[0][0] = (short)((cy * cz) >> 12);
    m->m[0][1] = (short)((-cy * sz) >> 12);
    m->m[0][2] = (short)(sy);

    m->m[1][0] = (short)((sx * sy * cz + (cx * sz << 12)) >> 24);
    m->m[1][1] = (short)((-sx * sy * sz + (cx * cz << 12)) >> 24);
    m->m[1][2] = (short)((-sx * cy) >> 12);

    m->m[2][0] = (short)((-cx * sy * cz + (sx * sz << 12)) >> 24);
    m->m[2][1] = (short)((cx * sy * sz + (sx * cz << 12)) >> 24);
    m->m[2][2] = (short)((cx * cy) >> 12);
#else
    // 32-bit version, less accurate but much faster on non-64bit CPUs
    int cx = rcos(r->vx);
    int sx = rsin(r->vx);
    int cy = rcos(r->vy);
    int sy = rsin(r->vy);
    int cz = rcos(r->vz);
    int sz = rsin(r->vz);

    m->m[0][0] = (short)((cy * cz) >> 12);
    m->m[0][1] = (short)((-cy * sz) >> 12);
    m->m[0][2] = (short)(sy);

    m->m[1][0] = (short)((((sx * sy) >> 12) * cz + (cx * sz)) >> 12);
    m->m[1][1] = (short)((((-sx * sy) >> 12) * sz + (cx * cz)) >> 12);
    m->m[1][2] = (short)((-sx * cy) >> 12);

    m->m[2][0] = (short)((((-cx * sy) >> 12) * cz + (sx * sz)) >> 12);
    m->m[2][1] = (short)((((cx * sy) >> 12) * sz + (sx * cz)) >> 12);
    m->m[2][2] = (short)((cx * cy) >> 12);
#endif

    m->t[0] = 0;
    m->t[1] = 0;
    m->t[2] = 0;

    return m;
}

MATRIX* RotMatrixY(long r, MATRIX* m) {
    int c = rcos((int)r);
    int s = rsin((int)r);
    int a0 = m->m[0][0], a1 = m->m[0][1], a2 = m->m[0][2];
    int b0 = m->m[2][0], b1 = m->m[2][1], b2 = m->m[2][2];

    m->m[0][0] = (short)((c * a0 + s * b0) >> 12);
    m->m[0][1] = (short)((c * a1 + s * b1) >> 12);
    m->m[0][2] = (short)((c * a2 + s * b2) >> 12);
    m->m[2][0] = (short)((c * b0 - s * a0) >> 12);
    m->m[2][1] = (short)((c * b1 - s * a1) >> 12);
    m->m[2][2] = (short)((c * b2 - s * a2) >> 12);
    return m;
}

MATRIX* RotMatrixX(long r, MATRIX* m) {
    int c = rcos((int)r);
    int s = rsin((int)r);
    int a0 = m->m[1][0], a1 = m->m[1][1], a2 = m->m[1][2];
    int b0 = m->m[2][0], b1 = m->m[2][1], b2 = m->m[2][2];

    m->m[1][0] = (short)((c * a0 - s * b0) >> 12);
    m->m[1][1] = (short)((c * a1 - s * b1) >> 12);
    m->m[1][2] = (short)((c * a2 - s * b2) >> 12);
    m->m[2][0] = (short)((s * a0 + c * b0) >> 12);
    m->m[2][1] = (short)((s * a1 + c * b1) >> 12);
    m->m[2][2] = (short)((s * a2 + c * b2) >> 12);
    return m;
}

MATRIX* RotMatrixZ(long r, MATRIX* m) {
    int c = rcos((int)r);
    int s = rsin((int)r);
    int a0 = m->m[0][0], a1 = m->m[0][1], a2 = m->m[0][2];
    int b0 = m->m[1][0], b1 = m->m[1][1], b2 = m->m[1][2];

    m->m[0][0] = (short)((c * a0 - s * b0) >> 12);
    m->m[0][1] = (short)((c * a1 - s * b1) >> 12);
    m->m[0][2] = (short)((c * a2 - s * b2) >> 12);
    m->m[1][0] = (short)((s * a0 + c * b0) >> 12);
    m->m[1][1] = (short)((s * a1 + c * b1) >> 12);
    m->m[1][2] = (short)((s * a2 + c * b2) >> 12);
    return m;
}

MATRIX* RotMatrixYXZ(SVECTOR* r, MATRIX* m) {
#ifdef PLATFORM_64BIT
    // 64-bit version, accurate with PS1 implementation
    long long cx = rcos(r->vx);
    long long sx = rsin(r->vx);
    long long cy = rcos(r->vy);
    long long sy = rsin(r->vy);
    long long cz = rcos(r->vz);
    long long sz = rsin(r->vz);

    m->m[0][0] = (short)(((cy * cz << 12) + sy * sx * sz) >> 24);
    m->m[0][1] = (short)(((-cy * sz << 12) + sy * sx * cz) >> 24);
    m->m[0][2] = (short)((sy * cx) >> 12);
    m->m[1][0] = (short)((cx * sz) >> 12);
    m->m[1][1] = (short)((cx * cz) >> 12);
    m->m[1][2] = (short)(-sx);
    m->m[2][0] = (short)(((-sy * cz << 12) + cy * sx * sz) >> 24);
    m->m[2][1] = (short)(((sy * sz << 12) + cy * sx * cz) >> 24);
    m->m[2][2] = (short)((cy * cx) >> 12);
#else
    // 32-bit version, less accurate but much faster on non-64bit CPUs
    int cx = rcos(r->vx);
    int sx = rsin(r->vx);
    int cy = rcos(r->vy);
    int sy = rsin(r->vy);
    int cz = rcos(r->vz);
    int sz = rsin(r->vz);

    m->m[0][0] = (short)((cy * cz + (((sy * sx) >> 12) * sz)) >> 12);
    m->m[0][1] = (short)((-cy * sz + (((sy * sx) >> 12) * cz)) >> 12);
    m->m[0][2] = (short)((sy * cx) >> 12);
    m->m[1][0] = (short)((cx * sz) >> 12);
    m->m[1][1] = (short)((cx * cz) >> 12);
    m->m[1][2] = (short)(-sx);
    m->m[2][0] = (short)((-sy * cz + (((cy * sx) >> 12) * sz)) >> 12);
    m->m[2][1] = (short)((sy * sz + (((cy * sx) >> 12) * cz)) >> 12);
    m->m[2][2] = (short)((cy * cx) >> 12);
#endif
    m->t[0] = 0;
    m->t[1] = 0;
    m->t[2] = 0;
    return m;
}

MATRIX* TransMatrix(MATRIX* m, VECTOR* v) {
    m->t[0] = v->vx;
    m->t[1] = v->vy;
    m->t[2] = v->vz;
    return m;
}

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v) {
    int sx = v->vx, sy = v->vy, sz = v->vz;
    int i;
    for (i = 0; i < 3; i++) {
        m->m[i][0] = (short)(((int)m->m[i][0] * sx) >> 12);
        m->m[i][1] = (short)(((int)m->m[i][1] * sy) >> 12);
        m->m[i][2] = (short)(((int)m->m[i][2] * sz) >> 12);
    }
    return m;
}

MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1) {
    MATRIX saved = M;
    MATRIX r;
    int j;

    M = *m0;
    for (j = 0; j < 3; j++) {
        V0.vx = m1->m[0][j];
        V0.vy = m1->m[1][j];
        V0.vz = m1->m[2][j];
        MVMVA(0x0086012); // sf=1, mx=0 (RT), v=0 (V0), cv=3 (none), lm=0
        r.m[0][j] = IR1;
        r.m[1][j] = IR2;
        r.m[2][j] = IR3;
    }
    M = saved;

    m0->m[0][0] = r.m[0][0];
    m0->m[0][1] = r.m[0][1];
    m0->m[0][2] = r.m[0][2];
    m0->m[1][0] = r.m[1][0];
    m0->m[1][1] = r.m[1][1];
    m0->m[1][2] = r.m[1][2];
    m0->m[2][0] = r.m[2][0];
    m0->m[2][1] = r.m[2][1];
    m0->m[2][2] = r.m[2][2];
    return m0;
}

// RTPS, RTPT, NCLIP, AVSZ3, AVSZ4 are implementations after
// https://problemkaputt.de/psxspx-gte-coordinate-calculation-commands.htm

// PSX divider table (UNR). Entry i estimates 1 / (1 + i/256) used in the
// Newton-Raphson step for the (H << 17) / SZ3 calculation in RTPS.
static const unsigned char unr_table[257] = {
    0xFF, 0xFD, 0xFB, 0xF9, 0xF7, 0xF5, 0xF3, 0xF1, 0xEF, 0xEE, 0xEC, 0xEA,
    0xE8, 0xE6, 0xE4, 0xE3, 0xE1, 0xDF, 0xDD, 0xDC, 0xDA, 0xD8, 0xD6, 0xD5,
    0xD3, 0xD1, 0xD0, 0xCE, 0xCD, 0xCB, 0xC9, 0xC8, 0xC6, 0xC5, 0xC3, 0xC1,
    0xC0, 0xBE, 0xBD, 0xBB, 0xBA, 0xB8, 0xB7, 0xB5, 0xB4, 0xB2, 0xB1, 0xB0,
    0xAE, 0xAD, 0xAB, 0xAA, 0xA9, 0xA7, 0xA6, 0xA4, 0xA3, 0xA2, 0xA0, 0x9F,
    0x9E, 0x9C, 0x9B, 0x9A, 0x99, 0x97, 0x96, 0x95, 0x94, 0x92, 0x91, 0x90,
    0x8F, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82,
    0x81, 0x7F, 0x7E, 0x7D, 0x7C, 0x7B, 0x7A, 0x79, 0x78, 0x77, 0x75, 0x74,
    0x73, 0x72, 0x71, 0x70, 0x6F, 0x6E, 0x6D, 0x6C, 0x6B, 0x6A, 0x69, 0x68,
    0x67, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x60, 0x5F, 0x5E, 0x5D, 0x5D,
    0x5C, 0x5B, 0x5A, 0x59, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x53, 0x52,
    0x51, 0x50, 0x4F, 0x4E, 0x4D, 0x4D, 0x4C, 0x4B, 0x4A, 0x49, 0x48, 0x48,
    0x47, 0x46, 0x45, 0x44, 0x43, 0x43, 0x42, 0x41, 0x40, 0x3F, 0x3F, 0x3E,
    0x3D, 0x3C, 0x3C, 0x3B, 0x3A, 0x39, 0x39, 0x38, 0x37, 0x36, 0x36, 0x35,
    0x34, 0x33, 0x33, 0x32, 0x31, 0x31, 0x30, 0x2F, 0x2E, 0x2E, 0x2D, 0x2C,
    0x2C, 0x2B, 0x2A, 0x2A, 0x29, 0x28, 0x28, 0x27, 0x26, 0x26, 0x25, 0x24,
    0x24, 0x23, 0x22, 0x22, 0x21, 0x20, 0x20, 0x1F, 0x1E, 0x1E, 0x1D, 0x1D,
    0x1C, 0x1B, 0x1B, 0x1A, 0x19, 0x19, 0x18, 0x18, 0x17, 0x16, 0x16, 0x15,
    0x15, 0x14, 0x14, 0x13, 0x12, 0x12, 0x11, 0x11, 0x10, 0x0F, 0x0F, 0x0E,
    0x0E, 0x0D, 0x0D, 0x0C, 0x0C, 0x0B, 0x0A, 0x0A, 0x09, 0x09, 0x08, 0x08,
    0x07, 0x07, 0x06, 0x06, 0x05, 0x05, 0x04, 0x04, 0x03, 0x03, 0x02, 0x02,
    0x01, 0x01, 0x00, 0x00, 0x00};

// PSX GTE divider: returns (H << 17) / SZ3 saturated to 1FFFFh, with the
// hardware's specific Newton-Raphson algorithm. Used by RTPS family.
static unsigned int gte_divide(unsigned short h, unsigned short sz3) {
    if (h >= sz3 * 2) {
        FLAG |= FLAG_DIV_OVF;
        return 0x1FFFF;
    }
    // Count leading zeros of sz3 within a 16-bit window. The early
    // h >= sz3*2 check above guarantees sz3 != 0 here
#if defined(__GNUC__) || defined(__clang__)
    unsigned z = (unsigned)__builtin_clz((unsigned)sz3) - 16u;
#else
    unsigned z = 0;
    unsigned x = sz3;
    while ((x & 0x8000) == 0) {
        x <<= 1;
        z++;
    }
#endif
    unsigned n = (unsigned)h << z;
    unsigned d = (unsigned)sz3 << z;
    unsigned u = unr_table[(d - 0x7FC0) >> 7] + 0x101;
    d = (0x2000080u - d * u) >> 8;
    d = (0x0000080u + d * u) >> 8;
    unsigned long long r = ((unsigned long long)n * d + 0x8000ull) >> 16;
    if (r > 0x1FFFFu)
        r = 0x1FFFFu;
    return (unsigned int)r;
}

// 44-bit MAC overflow check (MAC1..3). Real GTE has 44-bit accumulators;
// values outside +-(1<<43) set FLAG bits regardless of clamping. The full
// value still propagates into the SAR step (so >>sf is on the wrapped 44-bit).
static inline long long mac_check_44(long long v, unsigned mac_idx) {
    static const unsigned pos_bits[3] = {
        FLAG_MAC1_OVF_POS, FLAG_MAC2_OVF_POS, FLAG_MAC3_OVF_POS};
    static const unsigned neg_bits[3] = {
        FLAG_MAC1_OVF_NEG, FLAG_MAC2_OVF_NEG, FLAG_MAC3_OVF_NEG};
    if (v > 0x7FFFFFFFFFFLL)
        FLAG |= pos_bits[mac_idx];
    if (v < -0x80000000000LL)
        FLAG |= neg_bits[mac_idx];
    // Sign-extend from bit 43: cast to unsigned to make the left shift
    // well-defined, then arithmetic right-shift back to sign-extend.
    return (long long)((unsigned long long)v << 20) >> 20;
}

// MAC0 32-bit overflow check
static inline int mac0_check(long long v) {
    if (v > 0x7FFFFFFFLL)
        FLAG |= FLAG_MAC0_OVF_POS;
    if (v < -0x80000000LL)
        FLAG |= FLAG_MAC0_OVF_NEG;
    return (int)v;
}

// IR1..3 saturation. lm=1 clamps to 0..+7FFF, lm=0 clamps to -8000..+7FFF.
static inline short ir_saturate(int v, int lm, unsigned sat_flag) {
    int lo = lm ? 0 : -0x8000;
    int hi = 0x7FFF;
    if (v < lo || v > hi)
        FLAG |= sat_flag;
    if (v < lo)
        v = lo;
    if (v > hi)
        v = hi;
    return (short)v;
}

// Perspective Transformation helper.
// sf=1 → MAC1..3 are >>12 after multiply (typical "fixed" mode)
// sf=0 → MAC1..3 are >>0  (full-precision mode)
// lm   → IR saturation lower bound (0 if lm=1, else -8000)
// depth_cue: compute IR0 from DQA/DQB on the last vertex
static void RTPS_vertex(SVECTOR* v, int sf, int lm, int depth_cue) {
    int shift = sf ? 12 : 0;

    // MAC1..3 = (TR<<12 + RT*V) >> sf, with 44-bit overflow detection.
    long long m1 = ((long long)M.t[0] << 12) + (long long)M.m[0][0] * v->vx +
                   (long long)M.m[0][1] * v->vy + (long long)M.m[0][2] * v->vz;
    long long m2 = ((long long)M.t[1] << 12) + (long long)M.m[1][0] * v->vx +
                   (long long)M.m[1][1] * v->vy + (long long)M.m[1][2] * v->vz;
    long long m3 = ((long long)M.t[2] << 12) + (long long)M.m[2][0] * v->vx +
                   (long long)M.m[2][1] * v->vy + (long long)M.m[2][2] * v->vz;
    m1 = mac_check_44(m1, 0);
    m2 = mac_check_44(m2, 1);
    m3 = mac_check_44(m3, 2);
    MAC1 = (int)(m1 >> shift);
    MAC2 = (int)(m2 >> shift);
    MAC3 = (int)(m3 >> shift);

    IR1 = ir_saturate(MAC1, lm, FLAG_IR1_SAT);
    IR2 = ir_saturate(MAC2, lm, FLAG_IR2_SAT);
    // IR3 special: clamped to (-8000..7fff) or (0..7fff per lm), but the
    // FLAG bit is set based on (MAC3 SAR 12) vs -8000..7fff (without lm).
    {
        int v = MAC3;
        int sz_check = (int)(m3 >> 12);
        if (sz_check < -0x8000 || sz_check > 0x7FFF)
            FLAG |= FLAG_IR3_SAT;
        int lo = lm ? 0 : -0x8000;
        if (v < lo)
            v = lo;
        if (v > 0x7FFF)
            v = 0x7FFF;
        IR3 = (short)v;
    }

    // SZ FIFO push, then SZ3 = (m3 SAR 12) saturated to 0..ffff.
    SZ0 = SZ1;
    SZ1 = SZ2;
    SZ2 = SZ3;
    int sz_val = (int)(m3 >> 12);
    if (sz_val < 0 || sz_val > 0xFFFF)
        FLAG |= FLAG_SZ3_OTZ_SAT;
    if (sz_val < 0)
        sz_val = 0;
    if (sz_val > 0xFFFF)
        sz_val = 0xFFFF;
    SZ3 = (unsigned short)sz_val;

    int div_result = (int)gte_divide(H, SZ3);

    // SXY FIFO push, then SX2/SY2 from MAC0/10000h saturated to -400h..+3FFh.
    SX0 = SX1;
    SY0 = SY1;
    SX1 = SX2;
    SY1 = SY2;

    // psx-spx: MAC0 = (div_result*IR1) + OFX, with OFX in 16.16 fixed point
    // (so what's stored as integer pixels here gets shifted back up by 16).
    long long mac0 = (long long)div_result * IR1 + ((long long)OFX << 16);
    MAC0 = mac0_check(mac0);
    // SX2/SY2 saturation works on the un-truncated 64-bit MAC0 SAR 16, not on
    // the wrapped 32-bit MAC0 register. (psx-spx Lm_G1 acts before MAC0 wrap.)
    long long sx_full = mac0 >> 16;
    int sx = (sx_full < -0x400)  ? -0x400
             : (sx_full > 0x3FF) ? 0x3FF
                                 : (int)sx_full;
    if (sx_full < -0x400 || sx_full > 0x3FF)
        FLAG |= FLAG_SX2_SAT;
    SX2 = (short)sx;

    mac0 = (long long)div_result * IR2 + ((long long)OFY << 16);
    MAC0 = mac0_check(mac0);
    long long sy_full = mac0 >> 16;
    int sy = (sy_full < -0x400)  ? -0x400
             : (sy_full > 0x3FF) ? 0x3FF
                                 : (int)sy_full;
    if (sy_full < -0x400 || sy_full > 0x3FF)
        FLAG |= FLAG_SY2_SAT;
    SY2 = (short)sy;

    // SXP/SYP mirror SXY2 — read of reg 15 (SXYP) returns SXY2 value.
    SXP = SX2;
    SYP = SY2;

    if (depth_cue) {
        // MAC0 = div_result*DQA + DQB; IR0 = MAC0 >> 12 saturated 0..1000.
        long long m0 = (long long)div_result * DQA + (long long)DQB;
        MAC0 = mac0_check(m0);
        int ir0 = (int)(m0 >> 12);
        if (ir0 < 0 || ir0 > 0x1000)
            FLAG |= FLAG_IR0_SAT;
        if (ir0 < 0)
            ir0 = 0;
        if (ir0 > 0x1000)
            ir0 = 0x1000;
        IR0 = (short)ir0;
    }
}

// Perspective Transformation (single). cmd25 is the full 25-bit cop2 imm.
static void RTPS(unsigned int cmd25) {
    int sf = (cmd25 >> 19) & 1;
    int lm = (cmd25 >> 10) & 1;
    FLAG = 0;
    RTPS_vertex(&V0, sf, lm, 1);
    FLAG_update_error();
}

// Perspective Transformation (triple).
static void RTPT(unsigned int cmd25) {
    int sf = (cmd25 >> 19) & 1;
    int lm = (cmd25 >> 10) & 1;
    FLAG = 0;
    RTPS_vertex(&V0, sf, lm, 0);
    RTPS_vertex(&V1, sf, lm, 0);
    RTPS_vertex(&V2, sf, lm, 1);
    FLAG_update_error();
}

static void color_fifo_push(void);

// Matrix-vector multiply core used by MVMVA, NCS/NCT/NCDS/NCDT/NCCS/NCCT, etc.
// Selects matrix (mx), vector (vx), and translation (cv) per psx-spx encoding.
// Updates MAC1..3, IR1..3, and FLAG.
static inline void matrix_vec_mul(int sf, int lm, int mx, int vx, int cv) {
    int shift = sf ? 12 : 0;

    short M_sel[3][3];
    switch (mx) {
    case 0:
        M_sel[0][0] = M.m[0][0];
        M_sel[0][1] = M.m[0][1];
        M_sel[0][2] = M.m[0][2];
        M_sel[1][0] = M.m[1][0];
        M_sel[1][1] = M.m[1][1];
        M_sel[1][2] = M.m[1][2];
        M_sel[2][0] = M.m[2][0];
        M_sel[2][1] = M.m[2][1];
        M_sel[2][2] = M.m[2][2];
        break;
    case 1:
        M_sel[0][0] = L1.m[0][0];
        M_sel[0][1] = L1.m[0][1];
        M_sel[0][2] = L1.m[0][2];
        M_sel[1][0] = L1.m[1][0];
        M_sel[1][1] = L1.m[1][1];
        M_sel[1][2] = L1.m[1][2];
        M_sel[2][0] = L1.m[2][0];
        M_sel[2][1] = L1.m[2][1];
        M_sel[2][2] = L1.m[2][2];
        break;
    case 2:
        M_sel[0][0] = L2.m[0][0];
        M_sel[0][1] = L2.m[0][1];
        M_sel[0][2] = L2.m[0][2];
        M_sel[1][0] = L2.m[1][0];
        M_sel[1][1] = L2.m[1][1];
        M_sel[1][2] = L2.m[1][2];
        M_sel[2][0] = L2.m[2][0];
        M_sel[2][1] = L2.m[2][1];
        M_sel[2][2] = L2.m[2][2];
        break;
    default: {
        // mx=3 "garbage matrix" per psx-spx:
        //   row 0 = (-RGBC.R<<4,  RGBC.R<<4,  IR0)
        //   row 1 = ( R13,         R13,        R13)
        //   row 2 = ( R22,         R22,        R22)
        short r = (short)(((unsigned char*)&RGBC)[0] << 4);
        M_sel[0][0] = (short)-r;
        M_sel[0][1] = r;
        M_sel[0][2] = IR0;
        M_sel[1][0] = M_sel[1][1] = M_sel[1][2] = M.m[0][2];
        M_sel[2][0] = M_sel[2][1] = M_sel[2][2] = M.m[1][1];
        break;
    }
    }

    short Vx, Vy, Vz;
    switch (vx) {
    case 0:
        Vx = V0.vx;
        Vy = V0.vy;
        Vz = V0.vz;
        break;
    case 1:
        Vx = V1.vx;
        Vy = V1.vy;
        Vz = V1.vz;
        break;
    case 2:
        Vx = V2.vx;
        Vy = V2.vy;
        Vz = V2.vz;
        break;
    default:
        Vx = IR1;
        Vy = IR2;
        Vz = IR3;
        break;
    }

    int t1, t2, t3;
    switch (cv) {
    case 0:
        t1 = M.t[0];
        t2 = M.t[1];
        t3 = M.t[2];
        break;
    case 1:
        t1 = L1.t[0];
        t2 = L1.t[1];
        t3 = L1.t[2];
        break;
    case 2:
        t1 = L2.t[0];
        t2 = L2.t[1];
        t3 = L2.t[2];
        break;
    default:
        t1 = 0;
        t2 = 0;
        t3 = 0;
        break;
    }

    long long m1, m2, m3;
    if (cv == 2) {
        // FC bug: first multiplication FC<<12 + M[i][0]*V_x is computed, IR
        // gets saturated but is then DISCARDED; the result keeps only the
        // subsequent two M[i][1]*V_y + M[i][2]*V_z terms.
        long long tmp1 = ((long long)t1 << 12) + (long long)M_sel[0][0] * Vx;
        long long tmp2 = ((long long)t2 << 12) + (long long)M_sel[1][0] * Vx;
        long long tmp3 = ((long long)t3 << 12) + (long long)M_sel[2][0] * Vx;
        (void)mac_check_44(tmp1, 0);
        (void)mac_check_44(tmp2, 1);
        (void)mac_check_44(tmp3, 2);
        m1 = (long long)M_sel[0][1] * Vy + (long long)M_sel[0][2] * Vz;
        m2 = (long long)M_sel[1][1] * Vy + (long long)M_sel[1][2] * Vz;
        m3 = (long long)M_sel[2][1] * Vy + (long long)M_sel[2][2] * Vz;
    } else {
        m1 = ((long long)t1 << 12) + (long long)M_sel[0][0] * Vx +
             (long long)M_sel[0][1] * Vy + (long long)M_sel[0][2] * Vz;
        m2 = ((long long)t2 << 12) + (long long)M_sel[1][0] * Vx +
             (long long)M_sel[1][1] * Vy + (long long)M_sel[1][2] * Vz;
        m3 = ((long long)t3 << 12) + (long long)M_sel[2][0] * Vx +
             (long long)M_sel[2][1] * Vy + (long long)M_sel[2][2] * Vz;
    }
    m1 = mac_check_44(m1, 0);
    m2 = mac_check_44(m2, 1);
    m3 = mac_check_44(m3, 2);
    MAC1 = (int)(m1 >> shift);
    MAC2 = (int)(m2 >> shift);
    MAC3 = (int)(m3 >> shift);
    IR1 = ir_saturate(MAC1, lm, FLAG_IR1_SAT);
    IR2 = ir_saturate(MAC2, lm, FLAG_IR2_SAT);
    IR3 = ir_saturate(MAC3, lm, FLAG_IR3_SAT);
}

// Saturate to s16 (-8000..+7FFF) for the intermediate step of depth_cue.
// Input always fits in s32 after the >>shift in the caller (FC<<12 is up to
// s44 but >>12 brings it back to s32, and inN is already s32).
static inline short sat_s16(int v) {
    if (v < -0x8000)
        return -0x8000;
    if (v > 0x7FFF)
        return 0x7FFF;
    return (short)v;
}

// Depth-cue subroutine: MAC_n = inN + IR0 * sat_s16((FC_n<<12 - inN) >> sf*12),
// shifted right by sf*12. Used by DPCS/DPCT/DCPL/INTPL/NCDS/NCDT/CDP.
//
// Callers pass inN that fits in s32 ((RGB<<4)*IR, IR<<12, or RGB<<16). FC<<12
// is the only 44-bit-capable term; do that one step in s64, then drop back to
// s32 once the diff has been saturated to s16.
static inline void depth_cue(int sf, int lm, int inR, int inG, int inB) {
    int shift = sf ? 12 : 0;
    // sat_s16 of ((FC<<12 - inN) >> shift). Computed in s64 to handle the
    // pre-shift overflow safely; result after >>12 always fits in s32.
    short diff_r = sat_s16((int)((((long long)L2.t[0] << 12) - inR) >> shift));
    short diff_g = sat_s16((int)((((long long)L2.t[1] << 12) - inG) >> shift));
    short diff_b = sat_s16((int)((((long long)L2.t[2] << 12) - inB) >> shift));
    // IR0 * diff_s16 is s16*s16 → s32. inN is s32. Sum stays in s32 here
    // since (RGB<<16) + ((s16)0x7FFF*0x7FFF) is well within s32.
    MAC1 = (inR + (int)IR0 * diff_r) >> shift;
    MAC2 = (inG + (int)IR0 * diff_g) >> shift;
    MAC3 = (inB + (int)IR0 * diff_b) >> shift;
    IR1 = ir_saturate(MAC1, lm, FLAG_IR1_SAT);
    IR2 = ir_saturate(MAC2, lm, FLAG_IR2_SAT);
    IR3 = ir_saturate(MAC3, lm, FLAG_IR3_SAT);
}

// color_apply: MAC = (RGB << 4) * IR  >> sf*12. Used by CC, NCCS/NCCT.
// (RGB byte)<<4 fits in s12; * s16 IR fits in s28 → s32 multiply suffices.
static inline void color_apply(int sf, int lm) {
    int shift = sf ? 12 : 0;
    int r = ((unsigned char*)&RGBC)[0];
    int g = ((unsigned char*)&RGBC)[1];
    int b = ((unsigned char*)&RGBC)[2];
    MAC1 = ((r << 4) * IR1) >> shift;
    MAC2 = ((g << 4) * IR2) >> shift;
    MAC3 = ((b << 4) * IR3) >> shift;
    IR1 = ir_saturate(MAC1, lm, FLAG_IR1_SAT);
    IR2 = ir_saturate(MAC2, lm, FLAG_IR2_SAT);
    IR3 = ir_saturate(MAC3, lm, FLAG_IR3_SAT);
}

// depth_cue with (RGB<<4)*IR as input. Used by DCPL/NCDS/NCDT/CDP.
// (RGB<<4) is s12, * s16 IR fits in s28 — s32 suffices.
static inline void depth_cue_color(int sf, int lm) {
    int r = ((unsigned char*)&RGBC)[0];
    int g = ((unsigned char*)&RGBC)[1];
    int b = ((unsigned char*)&RGBC)[2];
    depth_cue(sf, lm, (r << 4) * IR1, (g << 4) * IR2, (b << 4) * IR3);
}

// SQR: square IR vector. MAC1..3 = IR1^2..IR3^2 (SAR sf*12), then IR=MAC.
// IR is s16; IR*IR fits in s32 (worst case 0x40000000), so no 44-bit math.
static void SQR(unsigned int cmd25) {
    int sf = (cmd25 >> 19) & 1;
    int lm = (cmd25 >> 10) & 1;
    int shift = sf ? 12 : 0;
    FLAG = 0;
    MAC1 = ((int)IR1 * IR1) >> shift;
    MAC2 = ((int)IR2 * IR2) >> shift;
    MAC3 = ((int)IR3 * IR3) >> shift;
    IR1 = ir_saturate(MAC1, lm, FLAG_IR1_SAT);
    IR2 = ir_saturate(MAC2, lm, FLAG_IR2_SAT);
    IR3 = ir_saturate(MAC3, lm, FLAG_IR3_SAT);
    FLAG_update_error();
}

// OP: outer product (cross product) of D = (R11, R22, R33) and IR.
// MAC1 = D2*IR3 - D3*IR2, MAC2 = D3*IR1 - D1*IR3, MAC3 = D1*IR2 - D2*IR1.
static void OP(unsigned int cmd25) {
    int sf = (cmd25 >> 19) & 1;
    int lm = (cmd25 >> 10) & 1;
    int shift = sf ? 12 : 0;
    int d1 = M.m[0][0], d2 = M.m[1][1], d3 = M.m[2][2];
    FLAG = 0;
    long long m1 = (long long)d2 * IR3 - (long long)d3 * IR2;
    long long m2 = (long long)d3 * IR1 - (long long)d1 * IR3;
    long long m3 = (long long)d1 * IR2 - (long long)d2 * IR1;
    m1 = mac_check_44(m1, 0);
    m2 = mac_check_44(m2, 1);
    m3 = mac_check_44(m3, 2);
    MAC1 = (int)(m1 >> shift);
    MAC2 = (int)(m2 >> shift);
    MAC3 = (int)(m3 >> shift);
    IR1 = ir_saturate(MAC1, lm, FLAG_IR1_SAT);
    IR2 = ir_saturate(MAC2, lm, FLAG_IR2_SAT);
    IR3 = ir_saturate(MAC3, lm, FLAG_IR3_SAT);
    FLAG_update_error();
}

// MVMVA: configurable matrix*vector + cv. Encoding bits 13-18 in cmd25.
static void MVMVA(unsigned int cmd25) {
    int sf = (cmd25 >> 19) & 1;
    int lm = (cmd25 >> 10) & 1;
    int mx = (cmd25 >> 17) & 3;
    int vx = (cmd25 >> 15) & 3;
    int cv = (cmd25 >> 13) & 3;
    FLAG = 0;
    matrix_vec_mul(sf, lm, mx, vx, cv);
    FLAG_update_error();
}

// NCS-style core: light_transform then color_matrix then push color.
static inline void ncs_core(int sf, int lm, int v) {
    matrix_vec_mul(sf, lm, 1, v, 3); // light transform: L1 * V_v + 0
    matrix_vec_mul(sf, lm, 2, 3, 1); // color matrix: L2 * IR + BK
    color_fifo_push();
}

// NCCS-style: light_transform + color_matrix + color_apply + push.
static inline void nccs_core(int sf, int lm, int v) {
    matrix_vec_mul(sf, lm, 1, v, 3);
    matrix_vec_mul(sf, lm, 2, 3, 1);
    color_apply(sf, lm);
    color_fifo_push();
}

// NCDS-style: light_transform + color_matrix + depth_cue + push.
static inline void ncds_core(int sf, int lm, int v) {
    matrix_vec_mul(sf, lm, 1, v, 3);
    matrix_vec_mul(sf, lm, 2, 3, 1);
    depth_cue_color(sf, lm);
    color_fifo_push();
}

static void NCS(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    ncs_core(sf, lm, 0);
    FLAG_update_error();
}
static void NCT(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    ncs_core(sf, lm, 0);
    ncs_core(sf, lm, 1);
    ncs_core(sf, lm, 2);
    FLAG_update_error();
}
static void NCCS(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    nccs_core(sf, lm, 0);
    FLAG_update_error();
}
static void NCCT(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    nccs_core(sf, lm, 0);
    nccs_core(sf, lm, 1);
    nccs_core(sf, lm, 2);
    FLAG_update_error();
}
static void NCDS(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    ncds_core(sf, lm, 0);
    FLAG_update_error();
}
static void NCDT(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    ncds_core(sf, lm, 0);
    ncds_core(sf, lm, 1);
    ncds_core(sf, lm, 2);
    FLAG_update_error();
}

// CC: color (no light transform). color_matrix + color_apply + push.
static void CC(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    matrix_vec_mul(sf, lm, 2, 3, 1);
    color_apply(sf, lm);
    color_fifo_push();
    FLAG_update_error();
}

// CDP: color depth cue (no light transform).
static void CDP(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    matrix_vec_mul(sf, lm, 2, 3, 1);
    depth_cue_color(sf, lm);
    color_fifo_push();
    FLAG_update_error();
}

// DPCS: depth cue single using RGBC<<16 as input.
static void DPCS(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    unsigned char* c = (unsigned char*)&RGBC;
    depth_cue(sf, lm, c[0] << 16, c[1] << 16, c[2] << 16);
    color_fifo_push();
    FLAG_update_error();
}

// DPCT: depth cue triple using RGB0<<16 as input (front of color FIFO).
static void DPCT(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    for (int i = 0; i < 3; i++) {
        int r = RGB0 & 0xFF;
        int g = (RGB0 >> 8) & 0xFF;
        int b = (RGB0 >> 16) & 0xFF;
        depth_cue(sf, lm, r << 16, g << 16, b << 16);
        color_fifo_push();
    }
    FLAG_update_error();
}

// DCPL: depth cue with pre-computed light (RGB<<4)*IR as input.
static void DCPL(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    depth_cue_color(sf, lm);
    color_fifo_push();
    FLAG_update_error();
}

// INTPL: interpolate IR toward FC using IR0. IR<<12 fits in s28 → s32 ok.
static void INTPL(unsigned int cmd) {
    int sf = (cmd >> 19) & 1, lm = (cmd >> 10) & 1;
    FLAG = 0;
    depth_cue(sf, lm, IR1 << 12, IR2 << 12, IR3 << 12);
    color_fifo_push();
    FLAG_update_error();
}

// Push the color FIFO: shift RGB0←RGB1, RGB1←RGB2, then RGB2 = packed
// (R,G,B,CODE) where R/G/B come from MAC1/MAC2/MAC3 >> 4 saturated to 0..0xFF,
// CODE byte copied from RGBC (cop1.6).
static void color_fifo_push(void) {
    int r = MAC1 >> 4, g = MAC2 >> 4, b = MAC3 >> 4;
    if (r < 0 || r > 0xFF)
        FLAG |= FLAG_COL_R_SAT;
    if (g < 0 || g > 0xFF)
        FLAG |= FLAG_COL_G_SAT;
    if (b < 0 || b > 0xFF)
        FLAG |= FLAG_COL_B_SAT;
    if (r < 0)
        r = 0;
    if (r > 0xFF)
        r = 0xFF;
    if (g < 0)
        g = 0;
    if (g > 0xFF)
        g = 0xFF;
    if (b < 0)
        b = 0;
    if (b > 0xFF)
        b = 0xFF;
    unsigned code = (*(unsigned int*)&RGBC >> 24) & 0xFF;
    RGB0 = RGB1;
    RGB1 = RGB2;
    RGB2 = ((unsigned)r) | (((unsigned)g) << 8) | (((unsigned)b) << 16) |
           (code << 24);
}

// GPF: general purpose interpolation (IR0 * IR -> MAC/IR, push color).
// MACn = (IR0 * IRn) SAR sf*12; IRn saturates; push color FIFO using MAC1..3.
// IR0 and IRn are s16 → s32 product (max 0x40000000), no 44-bit math.
static void GPF(unsigned int cmd25) {
    int sf = (cmd25 >> 19) & 1;
    int lm = (cmd25 >> 10) & 1;
    int shift = sf ? 12 : 0;
    FLAG = 0;
    MAC1 = ((int)IR0 * IR1) >> shift;
    MAC2 = ((int)IR0 * IR2) >> shift;
    MAC3 = ((int)IR0 * IR3) >> shift;
    IR1 = ir_saturate(MAC1, lm, FLAG_IR1_SAT);
    IR2 = ir_saturate(MAC2, lm, FLAG_IR2_SAT);
    IR3 = ir_saturate(MAC3, lm, FLAG_IR3_SAT);
    color_fifo_push();
    FLAG_update_error();
}

// GPL: general purpose interpolation with base (MAC + IR0 * IR -> MAC/IR).
// MACn = (MACn SHL sf*12 + IR0 * IRn) SAR sf*12; IRn saturates; push color.
static void GPL(unsigned int cmd25) {
    int sf = (cmd25 >> 19) & 1;
    int lm = (cmd25 >> 10) & 1;
    int shift = sf ? 12 : 0;
    FLAG = 0;
    long long m1 = ((long long)MAC1 << shift) + (long long)IR0 * IR1;
    long long m2 = ((long long)MAC2 << shift) + (long long)IR0 * IR2;
    long long m3 = ((long long)MAC3 << shift) + (long long)IR0 * IR3;
    m1 = mac_check_44(m1, 0);
    m2 = mac_check_44(m2, 1);
    m3 = mac_check_44(m3, 2);
    MAC1 = (int)(m1 >> shift);
    MAC2 = (int)(m2 >> shift);
    MAC3 = (int)(m3 >> shift);
    IR1 = ir_saturate(MAC1, lm, FLAG_IR1_SAT);
    IR2 = ir_saturate(MAC2, lm, FLAG_IR2_SAT);
    IR3 = ir_saturate(MAC3, lm, FLAG_IR3_SAT);
    color_fifo_push();
    FLAG_update_error();
}

// Normal clipping. MAC0 = SX0*(SY1-SY2) + SX1*(SY2-SY0) + SX2*(SY0-SY1).
// Each chained addition is checked at MAC0's 32-bit boundary.
static void NCLIP() {
    FLAG = 0;
    long long m = 0;
    m += (long long)SX0 * (SY1 - SY2);
    MAC0 = mac0_check(m);
    m += (long long)SX1 * (SY2 - SY0);
    MAC0 = mac0_check(m);
    m += (long long)SX2 * (SY0 - SY1);
    MAC0 = mac0_check(m);
    FLAG_update_error();
}

// Average of three Z values
static void AVSZ3() {
    FLAG = 0;
    MAC0 = ZSF3 * (SZ1 + SZ2 + SZ3);
    int otz = MAC0 >> 12;
    if (otz < 0 || otz > 0xFFFF) {
        FLAG |= FLAG_SZ3_OTZ_SAT;
    }
    OTZ = (unsigned short)(CLAMP(otz, 0, 0xFFFF));
    FLAG_update_error();
}

// Average of four Z values
static void AVSZ4() {
    FLAG = 0;
    MAC0 = ZSF4 * (SZ0 + SZ1 + SZ2 + SZ3);
    int otz = MAC0 >> 12;
    if (otz < 0 || otz > 0xFFFF) {
        FLAG |= FLAG_SZ3_OTZ_SAT;
    }
    OTZ = (unsigned short)(CLAMP(otz, 0, 0xFFFF));
    FLAG_update_error();
}

long AverageZ3(long sz0, long sz1, long sz2) {
    SZ1 = sz0;
    SZ2 = sz1;
    SZ3 = sz2;
    AVSZ3();
    return MAC0 >> 12;
}

long AverageZ4(long sz0, long sz1, long sz2, long sz3) {
    SZ0 = sz0;
    SZ1 = sz1;
    SZ2 = sz2;
    SZ3 = sz3;
    AVSZ4();
    return MAC0 >> 12;
}

void Psyz_GteStsxy(unsigned int* out) { *out = pack_xy(SXP, SYP); }

void Psyz_GteStsxy3(
    unsigned int* out0, unsigned int* out1, unsigned int* out2) {
    *out0 = pack_xy(SX0, SY0);
    *out1 = pack_xy(SX1, SY1);
    *out2 = pack_xy(SX2, SY2);
}

void Psyz_GteStsxy01c(unsigned int* out) {
    out[0] = pack_xy(SX0, SY0);
    out[1] = pack_xy(SX1, SY1);
}

void Psyz_GteStsxy3Gt3(void* polyGte) {
    POLY_GT3* poly = (POLY_GT3*)polyGte;
    poly->x0 = SX0;
    poly->y0 = SY0;
    poly->x1 = SX1;
    poly->y1 = SY1;
    poly->x2 = SX2;
    poly->y2 = SY2;
}

void Psyz_GteAvsz3(void) { AVSZ3(); }
void Psyz_GteDpcs(void) { DPCS(0x0780010); }
void Psyz_GteLcir(void) { MVMVA(0x04DE012); }
void Psyz_GteRtps(void) { RTPS(0x4A180001); }
void Psyz_GteRtpt(void) { RTPT(0x4A280030); }
void Psyz_GteNclip(void) { NCLIP(); }

void Psyz_GteLdv0(SVECTOR* v) {
    V0.vx = v->vx;
    V0.vy = v->vy;
    V0.vz = v->vz;
}

void Psyz_GteLdv3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2) {
    V0.vx = v0->vx;
    V0.vy = v0->vy;
    V0.vz = v0->vz;
    V1.vx = v1->vx;
    V1.vy = v1->vy;
    V1.vz = v1->vz;
    V2.vx = v2->vx;
    V2.vy = v2->vy;
    V2.vz = v2->vz;
}

void Psyz_GteLdv01c(SVECTOR* v) {
    V0.vx = v[0].vx;
    V0.vy = v[0].vy;
    V0.vz = v[0].vz;
    V1.vx = v[1].vx;
    V1.vy = v[1].vy;
    V1.vz = v[1].vz;
}

void Psyz_GteLdv3c(SVECTOR* v) {
    V0.vx = v[0].vx;
    V0.vy = v[0].vy;
    V0.vz = v[0].vz;
    V1.vx = v[1].vx;
    V1.vy = v[1].vy;
    V1.vz = v[1].vz;
    V2.vx = v[2].vx;
    V2.vy = v[2].vy;
    V2.vz = v[2].vz;
}

void Psyz_GteStszotz(unsigned int* out) {
    *out = (unsigned int)((int)SZ3 >> 2);
}
void Psyz_GteStopz(int* out) { *out = MAC0; }

long NormalClip(long sxy0, long sxy1, long sxy2) {
    // TODO can this be simplified with an union?
    SX0 = (short)sxy0;
    SY0 = (short)(sxy0 >> 16);
    SX1 = (short)sxy1;
    SY1 = (short)(sxy1 >> 16);
    SX2 = (short)sxy2;
    SY2 = (short)(sxy2 >> 16);
    NCLIP();
    return MAC0;
}

void NormalColorCol(SVECTOR* v0, CVECTOR* v1, CVECTOR* v2) {
    Psyz_GteLdv0(v0);
    Psyz_GteLdRgb(v1);
    NCCS(0x1B04084B);
    Psyz_GteStRgb(v2);
}

void DpqColor(CVECTOR* v0, long p, CVECTOR* v1) {
    Psyz_GteLdRgb(v0);
    IR0 = (short)p;
    DPCS(0x1000784A);
    Psyz_GteStRgb(v1);
}

void RotTrans(SVECTOR* v0, VECTOR* v1, int* flag) {
    V0 = *v0;
    MVMVA(0x0080012); // sf=1, mx=0 (RT), v=0 (V0), cv=0 (TR), lm=0
    v1->vx = MAC1;
    v1->vy = MAC2;
    v1->vz = MAC3;
    *flag = (int)FLAG;
}

void ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1) {
    // TODO unoptimized for 32-bit targets
    v1->vx = ((long long)m->m[0][0] * v0->vx + (long long)m->m[0][1] * v0->vy +
              (long long)m->m[0][2] * v0->vz) >>
             12;
    v1->vy = ((long long)m->m[1][0] * v0->vx + (long long)m->m[1][1] * v0->vy +
              (long long)m->m[1][2] * v0->vz) >>
             12;
    v1->vz = ((long long)m->m[2][0] * v0->vx + (long long)m->m[2][1] * v0->vy +
              (long long)m->m[2][2] * v0->vz) >>
             12;
}

void ApplyRotMatrix(SVECTOR* v0, VECTOR* v1) { ApplyMatrix(&M, v0, v1); }

long RotTransPers(SVECTOR* v0, int* sxy, int* p, int* flag) {
    V0 = *v0;
    RTPS(0x080001);
    *(unsigned int*)sxy = SX2 | (SY2 << 16);
    *p = IR0;
    *flag = (int)FLAG;
    return SZ3 >> 2;
}

long RotTransPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0, int* sxy1,
                   int* sxy2, int* p, int* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT(0x080030);
    *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
    *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
    *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
    *p = IR0;
    *flag = (int)FLAG;
    return SZ3 >> 2;
}

long RotTransPers4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0, int* sxy1,
    int* sxy2, int* sxy3, int* p, int* flag) {
    int flag1, flag2;
    RotTransPers3(v0, v1, v2, sxy0, sxy1, sxy2, p, &flag1);
    RotTransPers(v3, sxy3, p, &flag2);
    *flag = flag1 | flag2;
    return SZ3 >> 2;
}

long RotAverage3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0, int* sxy1,
                 int* sxy2, int* p, int* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT(0x080030);
    *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
    *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
    *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
    *flag = (int)FLAG;
    *p = IR0;
    AVSZ3();
    return OTZ;
}

long RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0,
                 int* sxy1, int* sxy2, int* sxy3, int* p, int* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT(0x080030);
    *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
    *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
    *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
    int flag1 = (int)FLAG;
    V0 = *v3;
    RTPS(0x080001);
    *(unsigned int*)sxy3 = SX2 | (SY2 << 16);
    *p = IR0;
    *flag = flag1 | (int)FLAG;
    AVSZ4();
    return OTZ;
}

long RotAverageNclip3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0,
                      int* sxy1, int* sxy2, int* p, int* otz, int* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT(0x080030);
    *flag = (int)FLAG;
    NCLIP();
    if (MAC0 > 0) {
        *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
        *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
        *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
        *p = IR0;
        AVSZ3();
        *otz = OTZ;
    }
    return MAC0;
}

long RotAverageNclip4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0, int* sxy1,
    int* sxy2, int* sxy3, int* p, int* otz, int* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT(0x080030);
    int flag1 = (int)FLAG;
    *flag = flag1;
    NCLIP();
    if (MAC0 > 0) {
        *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
        *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
        *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
        V0 = *v3;
        RTPS(0x080001);
        *(unsigned int*)sxy3 = SX2 | (SY2 << 16);
        *p = IR0;
        *flag = flag1 | (int)FLAG;
        AVSZ4();
        *otz = OTZ;
    }
    return MAC0;
}

/* Normalise to 4096 = 1.0, returning the SQUARE of the input length, which is
 * what PSY-Q's VectorNormal family does.
 *
 * These three were stubs returning 0 and leaving the output untouched. That is
 * not a harmless gap: DG_LookAt builds the camera basis with them
 * (source/libdg/display.c:274-275), so the view matrix's right and forward
 * rows kept whatever magnitude the raw eye-to-target difference had instead of
 * unit length, and DG_SetMainLightDir (libdg/light.c:75) read an output that
 * was never written.
 *
 * A 64-bit root is required rather than psyz's SquareRoot0: that takes a long,
 * and three squared 16-bit components reach ~3.2e9, past what a signed 32-bit
 * value holds -- the overflow would come back negative.
 *
 * Alias-safe on purpose: display.c:274 and takabe/dymc_flr.c:102 both pass the
 * same pointer as source and destination, so the inputs are read into locals
 * before anything is written back. */
static unsigned long long psyz_isqrt64(unsigned long long n) {
    unsigned long long root = 0;
    unsigned long long bit = 1ULL << 62;

    while (bit > n) {
        bit >>= 2;
    }
    while (bit) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

static long psyz_normalise(long long x, long long y, long long z, long long* ox,
                           long long* oy, long long* oz) {
    long long sq = x * x + y * y + z * z;
    long long len = (long long)psyz_isqrt64((unsigned long long)sq);

    if (len == 0) {
        *ox = *oy = *oz = 0;
        return 0;
    }
    *ox = (x << 12) / len;
    *oy = (y << 12) / len;
    *oz = (z << 12) / len;
    return (long)sq;
}

long VectorNormal(VECTOR* v0, VECTOR* v1) {
    long long nx, ny, nz;
    long sq = psyz_normalise(v0->vx, v0->vy, v0->vz, &nx, &ny, &nz);
    v1->vx = (long)nx;
    v1->vy = (long)ny;
    v1->vz = (long)nz;
    return sq;
}

long VectorNormalS(VECTOR* v0, SVECTOR* v1) {
    long long nx, ny, nz;
    long sq = psyz_normalise(v0->vx, v0->vy, v0->vz, &nx, &ny, &nz);
    /* the result is bounded by +/-4096, so a short never clips */
    v1->vx = (short)nx;
    v1->vy = (short)ny;
    v1->vz = (short)nz;
    return sq;
}

long VectorNormalSS(SVECTOR* v0, SVECTOR* v1) {
    long long nx, ny, nz;
    long sq = psyz_normalise(v0->vx, v0->vy, v0->vz, &nx, &ny, &nz);
    v1->vx = (short)nx;
    v1->vy = (short)ny;
    v1->vz = (short)nz;
    return sq;
}

MATRIX* TransposeMatrix(MATRIX* m0, MATRIX* m1) {
    NOT_IMPLEMENTED;
    return m0;
}

// External-emulator COP2/GTE bridge.
//
// Maps the canonical PSX GTE register file (data regs cop2.dataN, control regs
// cop2.ctrlN, and 25-bit COP2 commands) onto Psy-Z's internal high-level state.
// Intended for emulators integration: the emulator must catch the opcodes
// MTC2/CTC2/MFC2/CFC2/LWC2/SWC2 forwards them here.
//
// Register indices and naming follow
// https://psx-spx.consoledev.net/geometrytransformationenginegte/

static unsigned int pack_xy(short x, short y) {
    return ((unsigned int)(u16)x) | (((unsigned int)(u16)y) << 16);
}
static void unpack_xy(unsigned int v, short* x, short* y) {
    *x = (short)(v & 0xFFFF);
    *y = (short)((v >> 16) & 0xFFFF);
}

unsigned int Psyz_GteDataRead(unsigned idx) {
    switch (idx) {
    case 0:
        return ((unsigned int)(u16)V0.vx) | (((unsigned int)(u16)V0.vy) << 16);
    case 1:
        return (unsigned int)(int)V0.vz;
    case 2:
        return ((unsigned int)(u16)V1.vx) | (((unsigned int)(u16)V1.vy) << 16);
    case 3:
        return (unsigned int)(int)V1.vz;
    case 4:
        return ((unsigned int)(u16)V2.vx) | (((unsigned int)(u16)V2.vy) << 16);
    case 5:
        return (unsigned int)(int)V2.vz;
    case 6:
        return *(unsigned int*)&RGBC;
    case 7:
        return (unsigned int)OTZ;
    case 8:
        return (unsigned int)(int)IR0;
    case 9:
        return (unsigned int)(int)IR1;
    case 10:
        return (unsigned int)(int)IR2;
    case 11:
        return (unsigned int)(int)IR3;
    case 12:
        return pack_xy(SX0, SY0);
    case 13:
        return pack_xy(SX1, SY1);
    case 14:
        return pack_xy(SX2, SY2);
    case 15:
        return pack_xy(SXP, SYP);
    case 16:
        return (unsigned int)SZ0;
    case 17:
        return (unsigned int)SZ1;
    case 18:
        return (unsigned int)SZ2;
    case 19:
        return (unsigned int)SZ3;
    case 20:
        return RGB0;
    case 21:
        return RGB1;
    case 22:
        return RGB2;
    case 23:
        return RES1;
    case 24:
        return (unsigned int)MAC0;
    case 25:
        return (unsigned int)MAC1;
    case 26:
        return (unsigned int)MAC2;
    case 27:
        return (unsigned int)MAC3;
    default:
        return 0;
    }
}

void Psyz_GteDataWrite(unsigned idx, unsigned int v) {
    switch (idx) {
    case 0:
        V0.vx = (short)(v & 0xFFFF);
        V0.vy = (short)((v >> 16) & 0xFFFF);
        break;
    case 1:
        V0.vz = (short)(v & 0xFFFF);
        break;
    case 2:
        V1.vx = (short)(v & 0xFFFF);
        V1.vy = (short)((v >> 16) & 0xFFFF);
        break;
    case 3:
        V1.vz = (short)(v & 0xFFFF);
        break;
    case 4:
        V2.vx = (short)(v & 0xFFFF);
        V2.vy = (short)((v >> 16) & 0xFFFF);
        break;
    case 5:
        V2.vz = (short)(v & 0xFFFF);
        break;
    case 6:
        *(unsigned int*)&RGBC = v;
        break;
    case 7:
        OTZ = (u16)(v & 0xFFFF);
        break;
    case 8:
        IR0 = (short)(v & 0xFFFF);
        break;
    case 9:
        IR1 = (short)(v & 0xFFFF);
        break;
    case 10:
        IR2 = (short)(v & 0xFFFF);
        break;
    case 11:
        IR3 = (short)(v & 0xFFFF);
        break;
    case 12:
        unpack_xy(v, &SX0, &SY0);
        break;
    case 13:
        unpack_xy(v, &SX1, &SY1);
        break;
    case 14:
        unpack_xy(v, &SX2, &SY2);
        break;
    case 15:
        unpack_xy(v, &SXP, &SYP);
        break;
    case 16:
        SZ0 = (u16)(v & 0xFFFF);
        break;
    case 17:
        SZ1 = (u16)(v & 0xFFFF);
        break;
    case 18:
        SZ2 = (u16)(v & 0xFFFF);
        break;
    case 19:
        SZ3 = (u16)(v & 0xFFFF);
        break;
    case 20:
        RGB0 = v;
        break;
    case 21:
        RGB1 = v;
        break;
    case 22:
        RGB2 = v;
        break;
    case 23:
        RES1 = v;
        break;
    case 24:
        MAC0 = (int)v;
        break;
    case 25:
        MAC1 = (int)v;
        break;
    case 26:
        MAC2 = (int)v;
        break;
    case 27:
        MAC3 = (int)v;
        break;
    default:
        break;
    }
}

unsigned int Psyz_GteCtrlRead(unsigned idx) {
    switch (idx) {
    case 0:
        return ((unsigned int)(u16)M.m[0][0]) |
               (((unsigned int)(u16)M.m[0][1]) << 16);
    case 1:
        return ((unsigned int)(u16)M.m[0][2]) |
               (((unsigned int)(u16)M.m[1][0]) << 16);
    case 2:
        return ((unsigned int)(u16)M.m[1][1]) |
               (((unsigned int)(u16)M.m[1][2]) << 16);
    case 3:
        return ((unsigned int)(u16)M.m[2][0]) |
               (((unsigned int)(u16)M.m[2][1]) << 16);
    case 4:
        return (unsigned int)(int)M.m[2][2];
    case 5:
        return (unsigned int)M.t[0];
    case 6:
        return (unsigned int)M.t[1];
    case 7:
        return (unsigned int)M.t[2];
    case 8:
        return ((unsigned int)(u16)L1.m[0][0]) |
               (((unsigned int)(u16)L1.m[0][1]) << 16);
    case 9:
        return ((unsigned int)(u16)L1.m[0][2]) |
               (((unsigned int)(u16)L1.m[1][0]) << 16);
    case 10:
        return ((unsigned int)(u16)L1.m[1][1]) |
               (((unsigned int)(u16)L1.m[1][2]) << 16);
    case 11:
        return ((unsigned int)(u16)L1.m[2][0]) |
               (((unsigned int)(u16)L1.m[2][1]) << 16);
    case 12:
        return (unsigned int)(int)L1.m[2][2];
    case 13:
        return (unsigned int)L1.t[0];
    case 14:
        return (unsigned int)L1.t[1];
    case 15:
        return (unsigned int)L1.t[2];
    case 16:
        return ((unsigned int)(u16)L2.m[0][0]) |
               (((unsigned int)(u16)L2.m[0][1]) << 16);
    case 17:
        return ((unsigned int)(u16)L2.m[0][2]) |
               (((unsigned int)(u16)L2.m[1][0]) << 16);
    case 18:
        return ((unsigned int)(u16)L2.m[1][1]) |
               (((unsigned int)(u16)L2.m[1][2]) << 16);
    case 19:
        return ((unsigned int)(u16)L2.m[2][0]) |
               (((unsigned int)(u16)L2.m[2][1]) << 16);
    case 20:
        return (unsigned int)(int)L2.m[2][2];
    case 21:
        return (unsigned int)L2.t[0];
    case 22:
        return (unsigned int)L2.t[1];
    case 23:
        return (unsigned int)L2.t[2];
    case 24:
        return (unsigned int)(OFX << 16);
    case 25:
        return (unsigned int)(OFY << 16);
    case 26:
        return (unsigned int)H;
    case 27:
        return (unsigned int)(int)DQA;
    case 28:
        return (unsigned int)DQB;
    case 29:
        return (unsigned int)(int)ZSF3;
    case 30:
        return (unsigned int)(int)ZSF4;
    case 31:
        return FLAG;
    default:
        return 0;
    }
}

void Psyz_GteCtrlWrite(unsigned idx, unsigned int v) {
    switch (idx) {
    case 0:
        M.m[0][0] = (short)(v & 0xFFFF);
        M.m[0][1] = (short)((v >> 16) & 0xFFFF);
        break;
    case 1:
        M.m[0][2] = (short)(v & 0xFFFF);
        M.m[1][0] = (short)((v >> 16) & 0xFFFF);
        break;
    case 2:
        M.m[1][1] = (short)(v & 0xFFFF);
        M.m[1][2] = (short)((v >> 16) & 0xFFFF);
        break;
    case 3:
        M.m[2][0] = (short)(v & 0xFFFF);
        M.m[2][1] = (short)((v >> 16) & 0xFFFF);
        break;
    case 4:
        M.m[2][2] = (short)(v & 0xFFFF);
        break;
    case 5:
        M.t[0] = (int)v;
        break;
    case 6:
        M.t[1] = (int)v;
        break;
    case 7:
        M.t[2] = (int)v;
        break;
    case 8:
        L1.m[0][0] = (short)(v & 0xFFFF);
        L1.m[0][1] = (short)((v >> 16) & 0xFFFF);
        break;
    case 9:
        L1.m[0][2] = (short)(v & 0xFFFF);
        L1.m[1][0] = (short)((v >> 16) & 0xFFFF);
        break;
    case 10:
        L1.m[1][1] = (short)(v & 0xFFFF);
        L1.m[1][2] = (short)((v >> 16) & 0xFFFF);
        break;
    case 11:
        L1.m[2][0] = (short)(v & 0xFFFF);
        L1.m[2][1] = (short)((v >> 16) & 0xFFFF);
        break;
    case 12:
        L1.m[2][2] = (short)(v & 0xFFFF);
        break;
    case 13:
        L1.t[0] = (int)v;
        break;
    case 14:
        L1.t[1] = (int)v;
        break;
    case 15:
        L1.t[2] = (int)v;
        break;
    case 16:
        L2.m[0][0] = (short)(v & 0xFFFF);
        L2.m[0][1] = (short)((v >> 16) & 0xFFFF);
        break;
    case 17:
        L2.m[0][2] = (short)(v & 0xFFFF);
        L2.m[1][0] = (short)((v >> 16) & 0xFFFF);
        break;
    case 18:
        L2.m[1][1] = (short)(v & 0xFFFF);
        L2.m[1][2] = (short)((v >> 16) & 0xFFFF);
        break;
    case 19:
        L2.m[2][0] = (short)(v & 0xFFFF);
        L2.m[2][1] = (short)((v >> 16) & 0xFFFF);
        break;
    case 20:
        L2.m[2][2] = (short)(v & 0xFFFF);
        break;
    case 21:
        L2.t[0] = (int)v;
        break;
    case 22:
        L2.t[1] = (int)v;
        break;
    case 23:
        L2.t[2] = (int)v;
        break;
    // OFX/OFY are 20.12 fixed-point on the real GTE (PSY-Q's SetGeomOffset
    // writes `pixels << 16` into the ctrl reg). Psy-Z's RTPS math stores
    // them as integer pixels (it later does `OFX << 16` to align to 16.16),
    // so convert on the way in. Use arithmetic shift to keep sign.
    case 24:
        OFX = (int)v >> 16;
        break;
    case 25:
        OFY = (int)v >> 16;
        break;
    case 26:
        H = (u16)(v & 0xFFFF);
        break;
    case 27:
        DQA = (short)(v & 0xFFFF);
        break;
    case 28:
        DQB = (int)v;
        break;
    case 29:
        ZSF3 = (short)(v & 0xFFFF);
        break;
    case 30:
        ZSF4 = (short)(v & 0xFFFF);
        break;
    case 31:
        FLAG = v;
        break;
    default:
        break;
    }
}

void Psyz_GteCommand(unsigned int cmd) {
    // bits 0..5 select the op
    unsigned op = cmd & 0x3F;
    switch (op) {
    case 0x01:
        RTPS(cmd);
        break;
    case 0x06:
        NCLIP();
        break;
    case 0x0C:
        OP(cmd);
        break;
    case 0x10:
        DPCS(cmd);
        break;
    case 0x11:
        INTPL(cmd);
        break;
    case 0x12:
        MVMVA(cmd);
        break;
    case 0x13:
        NCDS(cmd);
        break;
    case 0x14:
        CDP(cmd);
        break;
    case 0x16:
        NCDT(cmd);
        break;
    case 0x1B:
        NCCS(cmd);
        break;
    case 0x1C:
        CC(cmd);
        break;
    case 0x1E:
        NCS(cmd);
        break;
    case 0x20:
        NCT(cmd);
        break;
    case 0x28:
        SQR(cmd);
        break;
    case 0x29:
        DCPL(cmd);
        break;
    case 0x2A:
        DPCT(cmd);
        break;
    case 0x2D:
        AVSZ3();
        break;
    case 0x2E:
        AVSZ4();
        break;
    case 0x30:
        RTPT(cmd);
        break;
    case 0x3D:
        GPF(cmd);
        break;
    case 0x3E:
        GPL(cmd);
        break;
    case 0x3F:
        NCCT(cmd);
        break;
    default:
        WARNF("unhandled GTE op:%02X", op);
        break;
    }
}

/* ---------------------------------------------------------------------------
 * PSY-Q inline GTE macros used by Metal Gear Solid but not previously exposed.
 *
 * Each one is transcribed from the MIPS COP2 assembly in the game's own
 * inline_n.h, which is the authoritative specification: the register numbers
 * below are exactly the ones that assembly loads and stores. Operations that
 * take a 25-bit command word are handed straight to the existing
 * implementations, the same way Psyz_GteRtpt already does.
 * ------------------------------------------------------------------------- */

/* --- operations (raw cop2 command words, taken from the game's asm) ------- */
void Psyz_GteRtir(void) { MVMVA(0x0049e012); }
void Psyz_GteRt(void) { MVMVA(0x00480012); }
void Psyz_GteSqr0(void) { SQR(0x00a00428); }
void Psyz_GteIntpl(void) { INTPL(0x00980011); }
void Psyz_GteNcs(void) { NCS(0x00c8041e); }
void Psyz_GteNctB(void) { NCT(0x00d80420); }
void Psyz_GteRtptB(void) { RTPT(0x00280030); }

/* --- loads --------------------------------------------------------------- */
/* IR1..IR3 from three halfwords; ldsv loads them unsigned (lhu), ldopv2SV
 * signed (lh) -- the only difference between the two. */
void Psyz_GteLdsv(const void* p) {
    const unsigned short* s = (const unsigned short*)p;
    IR1 = (short)s[0];
    IR2 = (short)s[1];
    IR3 = (short)s[2];
}
void Psyz_GteLdopv2SV(const void* p) {
    const short* s = (const short*)p;
    IR1 = s[0];
    IR2 = s[1];
    IR3 = s[2];
}
/* IR1..IR3 from three full words (lwc2 truncates into the 16-bit registers) */
void Psyz_GteLdlvl(const void* p) {
    const int* w = (const int*)p;
    IR1 = (short)w[0];
    IR2 = (short)w[1];
    IR3 = (short)w[2];
}
void Psyz_GteLddp(long v) { IR0 = (short)v; }
void Psyz_GteLdlzc(long v) {
    unsigned int u = (unsigned int)v;
    int n = 1;
    unsigned int sign = u & 0x80000000u;
    LZCS = (int)v;
    /* count leading zeros for a positive value, leading ones for a negative
     * one -- the PSX result is always in 1..32 */
    while (n < 32 && ((u << n) & 0x80000000u) == sign) {
        n++;
    }
    LZCR = n;
}
/* VXY0 is assembled from the halfwords at +0 and +4, VZ0 from the word at +8 */
void Psyz_GteLdlv0(const void* p) {
    const short* s = (const short*)p;
    V0.vx = s[0];
    V0.vy = s[2];
    V0.vz = (short)((const int*)p)[2];
}
void Psyz_GteLdsxy3(unsigned int xy0, unsigned int xy1, unsigned int xy2) {
    SX0 = (short)(xy0 & 0xFFFF), SY0 = (short)(xy0 >> 16);
    SX1 = (short)(xy1 & 0xFFFF), SY1 = (short)(xy1 >> 16);
    SX2 = (short)(xy2 & 0xFFFF), SY2 = (short)(xy2 >> 16);
}
/* far colour (cop2 control 21..23) from three halfwords */
void Psyz_GteLdIntpolSv0(const void* p) {
    const short* s = (const short*)p;
    L2.t[0] = s[0];
    L2.t[1] = s[1];
    L2.t[2] = s[2];
}

/* --- stores -------------------------------------------------------------- */
void Psyz_GteStsv(void* p) {
    short* s = (short*)p;
    s[0] = IR1;
    s[1] = IR2;
    s[2] = IR3;
}
void Psyz_GteStsz(unsigned int* p) { *p = SZ3; }
void Psyz_GteStsz3(unsigned int* p0, unsigned int* p1, unsigned int* p2) {
    *p0 = SZ1;
    *p1 = SZ2;
    *p2 = SZ3;
}
void Psyz_GteStsz3c(unsigned int* p) {
    p[0] = SZ1;
    p[1] = SZ2;
    p[2] = SZ3;
}
void Psyz_GteStsxy0(unsigned int* p) { *p = pack_xy(SX0, SY0); }
void Psyz_GteStsxy1(unsigned int* p) { *p = pack_xy(SX1, SY1); }
void Psyz_GteStsxy2(unsigned int* p) { *p = pack_xy(SX2, SY2); }
void Psyz_GteStsxy01(unsigned int* p0, unsigned int* p1) {
    *p0 = pack_xy(SX0, SY0);
    *p1 = pack_xy(SX1, SY1);
}
void Psyz_GteStsxy3c(unsigned int* p) {
    p[0] = pack_xy(SX0, SY0);
    p[1] = pack_xy(SX1, SY1);
    p[2] = pack_xy(SX2, SY2);
}
/* MAC1..MAC3 -- "lvnl" is the long vector, not the saturated IR copies */
void Psyz_GteStlvnl(int* p) {
    p[0] = MAC1;
    p[1] = MAC2;
    p[2] = MAC3;
}
void Psyz_GteStlvnl0(int* p) { *p = MAC1; }
void Psyz_GteStlvnl1(int* p) { *p = MAC2; }
void Psyz_GteStlvnl2(int* p) { *p = MAC3; }
void Psyz_GteStlzc(int* p) { *p = LZCR; }
void Psyz_GteStrgb3(unsigned int* p0, unsigned int* p1, unsigned int* p2) {
    *p0 = RGB0;
    *p1 = RGB1;
    *p2 = RGB2;
}

/* --- control registers --------------------------------------------------- */
/* cop2 control 0..7 is exactly the packed MATRIX image the game expects */
void Psyz_GteReadRotMatrix(MATRIX* m) { *m = M; }
/* cop2 control 8..12: the light matrix 3x3 only, no translation */
void Psyz_GteSetLightMatrix(const MATRIX* m) {
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            L1.m[i][j] = m->m[i][j];
        }
    }
}

/* --- MGS's inline_x.h: direct loads of the V0..V2 vertex registers -------- */
void Psyz_GteReadOpz(int* out) { *out = MAC0; }
void Psyz_GteLdVXY0(unsigned int xy) {
    V0.vx = (short)(xy & 0xFFFF), V0.vy = (short)(xy >> 16);
}
void Psyz_GteLdVXY1(unsigned int xy) {
    V1.vx = (short)(xy & 0xFFFF), V1.vy = (short)(xy >> 16);
}
void Psyz_GteLdVXY2(unsigned int xy) {
    V2.vx = (short)(xy & 0xFFFF), V2.vy = (short)(xy >> 16);
}
void Psyz_GteLdVZ0(int z) { V0.vz = (short)z; }
void Psyz_GteLdVZ1(int z) { V1.vz = (short)z; }
void Psyz_GteLdVZ2(int z) { V2.vz = (short)z; }

/* --- matrix helpers and per-vector rotates the PSY-Q inline set exposed ---- */
SVECTOR* ApplyMatrixSV(MATRIX* m, SVECTOR* v0, SVECTOR* v1) {
    long x = v0->vx, y = v0->vy, z = v0->vz;
    v1->vx = (short)((m->m[0][0] * x + m->m[0][1] * y + m->m[0][2] * z) >> 12);
    v1->vy = (short)((m->m[1][0] * x + m->m[1][1] * y + m->m[1][2] * z) >> 12);
    v1->vz = (short)((m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z) >> 12);
    return v1;
}
VECTOR* ApplyRotMatrixLV(VECTOR* v0, VECTOR* v1) {
    long x = v0->vx, y = v0->vy, z = v0->vz;
    v1->vx = (M.m[0][0] * x + M.m[0][1] * y + M.m[0][2] * z) >> 12;
    v1->vy = (M.m[1][0] * x + M.m[1][1] * y + M.m[1][2] * z) >> 12;
    v1->vz = (M.m[2][0] * x + M.m[2][1] * y + M.m[2][2] * z) >> 12;
    return v1;
}
/* Rotate one of the three loaded vertices: MVMVA with v selecting V0/V1/V2.
 *
 * These three were transcribed from the wrong ROW of the game's own table.
 * MGS spells out every variant it uses in source/include/inline_n.h, and the
 * plain forms differ from the +TR and +BK forms only in the cv field:
 *
 *     gte_rtv0_b   0x00486012      gte_rtv0tr_b 0x00480012   <- was here
 *     gte_rtv1_b   0x0048e012      gte_rtv1bk_b 0x0048a012   <- was here
 *     gte_rtv2_b   0x00496012      gte_rtv2tr_b 0x00490012   <- was here
 *
 * So every "rotate this vector" silently became "rotate it and add the
 * translation vector still sitting in the GTE" -- a stale offset of hundreds
 * to thousands of world units added to results that were supposed to be pure
 * directions. Psyz_GteRt above keeps 0x00480012 on purpose: gte_rt_b really
 * is that word. */
void Psyz_GteRtv0(void) { MVMVA(0x00486012); }
void Psyz_GteRtv1(void) { MVMVA(0x0048e012); }
void Psyz_GteRtv2(void) { MVMVA(0x00496012); }

/* --- the last few PSY-Q inline forms MGS reaches for ---------------------- */
void Psyz_GteNccs(void) { NCCS(0x00c8041b); }
/* Light-matrix forms, from the same table:
 *     gte_ll_b     0x004a6412      gte_llv0_b   0x004a6012
 * Ll had 0x0098012 -- seven hex digits, not even a well-formed command word --
 * and Llv0 had rtv0's, so lighting was being computed with the rotation
 * matrix instead of the light matrix. */
void Psyz_GteLl(void) { MVMVA(0x004a6412); }
void Psyz_GteLlv0(void) { MVMVA(0x004a6012); }
void Psyz_GteIntplB(void) { INTPL(0x00980011); }
/* far-colour direction: the three FC control registers as full words */
void Psyz_GteLdfcdir(const void* p) {
    const int* w = (const int*)p;
    L2.t[0] = w[0];
    L2.t[1] = w[1];
    L2.t[2] = w[2];
}
/* three-scalar spelling of the far-colour direction load */
void Psyz_GteLdfcdir3(long r, long g, long b) {
    L2.t[0] = (int)r;
    L2.t[1] = (int)g;
    L2.t[2] = (int)b;
}

/* --- the last inline forms, defined per-file by MGS's game code ----------- */
/* far colour, x and z only (control 21 and 23) */
void Psyz_GteLdIntpolSv0Xz(const void* p) {
    const short* s = (const short*)p;
    L2.t[0] = s[0];
    L2.t[2] = s[2];
}
/* IR1 and IR3 from halfwords at +0 and +4 */
void Psyz_GteLdIntpolSv1Xz(const void* p) {
    const unsigned short* s = (const unsigned short*)p;
    IR1 = (short)s[0];
    IR3 = (short)s[2];
}
/* VXY0 from a single packed word */
void Psyz_GteLdv0h(const void* p) {
    unsigned int xy = *(const unsigned int*)p;
    V0.vx = (short)(xy & 0xFFFF);
    V0.vy = (short)(xy >> 16);
}
int Psyz_GtePopColor(void) { return IR1; }
void Psyz_GteReadNormal(int* x, int* y, int* z) {
    *x = IR1;
    *y = IR2;
    *z = IR3;
}
/* IR1, IR2 as halfwords */
void Psyz_GteStbh(void* p) {
    short* s = (short*)p;
    s[0] = IR1;
    s[1] = IR2;
}
/* IR1 at +0, zero at +2, IR3 at +4 */
void Psyz_GteStsvXz(void* p) {
    short* s = (short*)p;
    s[0] = IR1;
    s[1] = 0;
    s[2] = IR3;
}
