/* PSY-Q SDK entry points that PSY-Z does not provide, implemented for the
 * portable build of Metal Gear Solid.
 *
 * Split by confidence:
 *   - the geometry helpers are real implementations, transcribed from the
 *     documented PSY-Q semantics and using PSY-Z's own fixed-point primitives
 *   - the kernel thread/exec API is stubbed and LOUD: MGS's mts layer sits on
 *     top of it, so the right time to implement it for real is when a boot
 *     actually reaches one of these, and the abort message says which
 */

#include <psyz.h>
#include <libgte.h>
#include <libgpu.h>
#include <stdio.h>
#include <stdlib.h>

/* --------------------------------------------------------------------------
 * Geometry
 * ------------------------------------------------------------------------ */

/* m2 = m0 x m1 (rotation part only; PSY-Q leaves the translation alone) */
MATRIX* MulMatrix0(MATRIX* m0, MATRIX* m1, MATRIX* m2) {
    MATRIX r;
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            int acc = m0->m[i][0] * m1->m[0][j] + m0->m[i][1] * m1->m[1][j] +
                      m0->m[i][2] * m1->m[2][j];
            r.m[i][j] = (short)(acc >> 12);
        }
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            m2->m[i][j] = r.m[i][j];
        }
    }
    return m2;
}

/* m1 = m0 x m1, in place */
MATRIX* MulMatrix2(MATRIX* m0, MATRIX* m1) { return MulMatrix0(m0, m1, m1); }

/* current GTE rotation matrix = itself x m0
 *
 * PSY-Q's reference says m0 = R_gte x m0 with the GTE left alone, so this is
 * suspect. But changing it to that -- together with dropping SetRotMatrix from
 * the _gte builders, which the analysis said had to be done as a pair -- made
 * every character skeleton fall apart on the board. Reverted, and left as it
 * was until the limb transform chain in libdg/screen.c and libdg/pos.c is read
 * properly rather than patched from a reference manual. */
MATRIX* MulRotMatrix(MATRIX* m0) {
    MATRIX cur;
    ReadRotMatrix(&cur);
    MulMatrix0(&cur, m0, &cur);
    SetRotMatrix(&cur);
    return m0;
}

MATRIX* ReadRotMatrix(MATRIX* m) {
    Psyz_GteReadRotMatrix(m);
    return m;
}

/* cross product; the "12" form keeps the 20.12 fixed-point scale */
VECTOR* OuterProduct0(VECTOR* v0, VECTOR* v1, VECTOR* v2) {
    long x = v0->vy * v1->vz - v0->vz * v1->vy;
    long y = v0->vz * v1->vx - v0->vx * v1->vz;
    long z = v0->vx * v1->vy - v0->vy * v1->vx;
    v2->vx = x;
    v2->vy = y;
    v2->vz = z;
    return v2;
}

VECTOR* OuterProduct12(VECTOR* v0, VECTOR* v1, VECTOR* v2) {
    long x = v0->vy * v1->vz - v0->vz * v1->vy;
    long y = v0->vz * v1->vx - v0->vx * v1->vz;
    long z = v0->vx * v1->vy - v0->vy * v1->vx;
    v2->vx = x >> 12;
    v2->vy = y >> 12;
    v2->vz = z >> 12;
    return v2;
}

/* Componentwise square. The trailing 0 is PSY-Q's shift factor, and it means
 * NO shift -- exactly like OuterProduct0 above, and unlike the *12 spellings
 * which are the ones that come back >>12.
 *
 * This shifted, so every square was 4096x too small. Everything that measures
 * a distance in this game goes Square0 -> sum -> SquareRoot0, so every length
 * came back divided by 64 (sqrt of 4096). GV_LenVec3 then scales a vector by
 * len2/len1 with that len1 as the divisor, which made the movement step 64x
 * too large: Snake advanced three frames, overshot into a wall, and the
 * collision response threw him back -- a sawtooth that never made progress.
 * The same wrong lengths also reached enemy sight ranges, bullet hits and the
 * lighting attenuation in libdg/light.c. */
VECTOR* Square0(VECTOR* v0, VECTOR* v1) {
    v1->vx = v0->vx * v0->vx;
    v1->vy = v0->vy * v0->vy;
    v1->vz = v0->vz * v0->vz;
    return v1;
}

VECTOR* Square12(VECTOR* v0, VECTOR* v1) {
    v1->vx = (v0->vx * v0->vx) >> 12;
    v1->vy = (v0->vy * v0->vy) >> 12;
    v1->vz = (v0->vz * v0->vz) >> 12;
    return v1;
}

/* The *_gte spellings build the matrix and load it into the GTE in one go.
 * Rotation order follows the name; RotMatrix is Z*Y*X like PSY-Q's default. */
static void build_rot(SVECTOR* r, MATRIX* m, int order_yxz) {
    int sx = rsin(r->vx), cx = rcos(r->vx);
    int sy = rsin(r->vy), cy = rcos(r->vy);
    int sz = rsin(r->vz), cz = rcos(r->vz);
    MATRIX mx, my, mz, t;

    mx.m[0][0] = 4096; mx.m[0][1] = 0;   mx.m[0][2] = 0;
    mx.m[1][0] = 0;    mx.m[1][1] = cx;  mx.m[1][2] = -sx;
    mx.m[2][0] = 0;    mx.m[2][1] = sx;  mx.m[2][2] = cx;

    my.m[0][0] = cy;   my.m[0][1] = 0;   my.m[0][2] = sy;
    my.m[1][0] = 0;    my.m[1][1] = 4096; my.m[1][2] = 0;
    my.m[2][0] = -sy;  my.m[2][1] = 0;   my.m[2][2] = cy;

    mz.m[0][0] = cz;   mz.m[0][1] = -sz; mz.m[0][2] = 0;
    mz.m[1][0] = sz;   mz.m[1][1] = cz;  mz.m[1][2] = 0;
    mz.m[2][0] = 0;    mz.m[2][1] = 0;   mz.m[2][2] = 4096;

    if (order_yxz) {
        MulMatrix0(&my, &mx, &t);
        MulMatrix0(&t, &mz, m);
    } else {
        MulMatrix0(&mz, &my, &t);
        MulMatrix0(&t, &mx, m);
    }
}

/* The _gte spellings build the matrix USING the GTE, and PSY-Q leaves the
 * result sitting in the GTE's rotation register as a documented side effect --
 * that is the whole point of the variant, and callers on the character path
 * (libdg/screen.c) rely on it. Removing the SetRotMatrix here was tried and
 * scrambled every skeleton on screen: legs and arms reversed, head inside the
 * chest. Leave it. */
MATRIX* RotMatrix_gte(SVECTOR* r, MATRIX* m) {
    build_rot(r, m, 0);
    SetRotMatrix(m);
    return m;
}

MATRIX* RotMatrixZYX_gte(SVECTOR* r, MATRIX* m) {
    build_rot(r, m, 0);
    SetRotMatrix(m);
    return m;
}

MATRIX* RotMatrixYXZ_gte(SVECTOR* r, MATRIX* m) {
    build_rot(r, m, 1);
    SetRotMatrix(m);
    return m;
}

/* --------------------------------------------------------------------------
 * GPU odds and ends
 * ------------------------------------------------------------------------ */

/* GPU_printf is defined by the PSY-Q decomp's libgpu/sys.c */

/* the 2-suffixed forms are the non-blocking variants; with a software GPU the
 * transfer is synchronous either way */
int LoadImage2(RECT* rect, u_long* p) {
    LoadImage(rect, p);
    return 0;
}

int StoreImage2(RECT* rect, u_long* p) {
    StoreImage(rect, p);
    return 0;
}

void SetDrawStp(DR_STP* p, int stp) {
    /* Build the mask-bit primitive; the rasterizer already honours DR_STP.
     *
     * The GP0 word goes in code[0], NOT at raw word index 1. On the console a
     * DR_STP is one packed tag followed by the command, so [1] was right; PSY-Z
     * splits the tag into { u_long tag; u_long len }, which makes [1] the
     * LENGTH field. Writing 0xE6000xxx there told the queue reader the packet
     * was 3.8 billion words long -- it dropped the packet and then resumed
     * mid-primitive, reading vertices and pointers as commands. That is the
     * screen going white, and it fires on combat because muzzle flashes and
     * smoke are what turn semi-transparency on.
     *
     * psyz hit exactly this in its own setDrawTPage and fixed it there (see the
     * [2] and the comment in libgpu.h); SetDrawStp lives in MGS's compat layer
     * so it never got the same treatment. Use the struct member rather than an
     * index, so it cannot drift again if the header changes. */
    setlen(p, 1);
    p->code[0] = 0xE6000000 | (stp & 3);
}

u_long* BreakDraw(void) {
    return 0; /* abort the in-flight DMA; nothing is in flight here */
}

void StUnSetRing(void) { /* CD streaming ring teardown; no ring yet */ }

/* --------------------------------------------------------------------------
 * Pad
 * ------------------------------------------------------------------------ */

/* PadInitDirect lives in esp32/main/esp_input.c: it registers the game's
 * receive buffers so the board's buttons can be written into them */


void PadStopCom(void) {}

int PadInfoAct(int port, int actno, int term) {
    (void)port;
    (void)actno;
    (void)term;
    return 0; /* no actuators (rumble) modelled */
}

int SafetyCheck(void) { return 0; }

/* --------------------------------------------------------------------------
 * Kernel: threads and executable control
 *
 * MGS's mts task layer is built on these. They are deliberately loud rather
 * than silently wrong: whichever one a boot reaches first is the next thing to
 * implement, and a silent stub here would produce a hang with no clue.
 * ------------------------------------------------------------------------ */

static void unimplemented(const char* what) {
    printf("\n*** PSX kernel call not implemented yet: %s()\n", what);
    printf("*** MGS's mts task layer needs this. Implement it before the "
           "boot can proceed past here.\n");
    abort();
}

/* OpenTh / CloseTh / ChangeTh now live in port/esp32_threads.c,
 * implemented on FreeRTOS tasks. */

unsigned long GetGp(void) { return 0; }

unsigned long GetSp(void) {
    int here;
    return (unsigned long)(size_t)&here;
}

long SetConf(unsigned long ev, unsigned long tcb, unsigned long sp) {
    (void)ev;
    (void)tcb;
    (void)sp;
    return 0; /* kernel table sizing; ours are fixed */
}



void SetMem(unsigned long n) { (void)n; /* 2 or 8 MB RAM declaration */ }

void LoadExec(char* name, unsigned long s_addr, unsigned long s_size) {
    (void)s_addr;
    (void)s_size;
    printf("\n*** LoadExec(\"%s\"): the game asked to chain-load another "
           "executable.\n", name ? name : "(null)");
    printf("*** On the PSX this is how discs swap; here it means the boot "
           "path reached a disc change.\n");
    abort();
}

/* On the PSX these masked interrupts; mts wraps every piece of scheduler
 * bookkeeping in them. Off-console the "interrupt" is the vblank tick task,
 * which checks this depth and defers its callback while any critical section
 * is open -- leaving these as no-ops let the tick preempt mts mid-update and
 * the cooperative state tore itself apart, wedging the boot at random points.
 * A counter, not a flag: the sections nest. */
extern volatile int psyz_critical_depth;   /* psyz/src/psyz/libapi.c */

void SwEnterCriticalSection(void) { psyz_critical_depth = 1; }
void SwExitCriticalSection(void) { psyz_critical_depth = 0; }

/* linker-provided markers the startup code references */
unsigned char _bss_orgend[1];
void _96_init(void) {}

/* --------------------------------------------------------------------------
 * Odds and ends the link still asks for
 * ------------------------------------------------------------------------ */

/* Root counters: the PSX's three hardware timers. MGS reads RCnt1 to measure
 * how long the GPU took; a monotonic tick is enough until the ESP-IDF timer is
 * wired in. */
long GetRCnt(unsigned long spec) {
    extern unsigned int mgs_frame_count;
    (void)spec;
    return (long)(mgs_frame_count * 1000u);
}

/* MGS's own pad bring-up hook; the platform layer owns the actual polling. */
void MyPadInit(void) {}

/* PC* host-file access comes from the PSY-Q decomp's libapi/ */

/* MGS implements PCinit/PCopen/PCread/PCclose itself in libfs/select.c but not
 * these three; they are the write side of the PSY-Q host-file link, unused off
 * a development PSX. */
int PCcreat(const char* name, int perm) { (void)name; (void)perm; return -1; }
int PCwrite(int fd, char* buf, int len) {
    (void)fd; (void)buf; (void)len; return -1;
}
int PClseek(int fd, int offset, int flag) {
    (void)fd; (void)offset; (void)flag; return -1;
}
