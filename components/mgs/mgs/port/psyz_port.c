/* Support definitions for building MGS against PSY-Z instead of the PSY-Q SDK.
 * Kept in one translation unit so nothing here depends on engine headers. */

#include <psyz.h>
#include <libgte.h>

/* The PSX's 1 KiB scratchpad at 0x1f800000. The engine addresses it through
 * SCRPAD_ADDR (see source/include/psxdefs.h), so relocating it is just this
 * array. Aligned generously: the engine casts it to several structs. */
unsigned long psyz_scratchpad[256] __attribute__((aligned(16)));

/* PSY-Q's CompMatrix multiplies three matrices; PSY-Z declares a different
 * signature under the same name, so MGS reaches this wrapper instead.
 *
 * m2 = m0 o m1, which for the translation means m0's ROTATION applied to m1's
 * offset, then m0's own offset added. Getting that rotation is the whole point:
 * this is what walks a skeleton down its chain, so if the offset is added
 * unrotated every bone still spins correctly about a joint that never moves --
 * limbs detach from the torso and knees bend the wrong way, while the pose
 * looks perfect the moment every angle is zero.
 *
 * That is exactly what happened here. The first version wrote the translation
 * through a (VECTOR*) cast onto MATRIX::t, which is a strict-aliasing
 * violation; at -O2 GCC is entitled to assume those stores cannot be read back
 * through the long[3] member, and it deleted the rotation outright. The
 * disassembly showed a CompMatrix with no multiply anywhere in it. Keep the
 * arithmetic here in plain longs with no type punning so it cannot happen
 * again. The accumulator is 64-bit to mirror the GTE's 44-bit MACs -- a world
 * offset of 100k units times 4096, summed three times, does not fit in 32.
 */
MATRIX* CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2)
{
    MATRIX    tmp;
    long long x = m1->t[0], y = m1->t[1], z = m1->t[2];

    MulMatrix0(m0, m1, &tmp);

    tmp.t[0] = (long)(((m0->m[0][0] * x + m0->m[0][1] * y + m0->m[0][2] * z) >>
                       12) + m0->t[0]);
    tmp.t[1] = (long)(((m0->m[1][0] * x + m0->m[1][1] * y + m0->m[1][2] * z) >>
                       12) + m0->t[1]);
    tmp.t[2] = (long)(((m0->m[2][0] * x + m0->m[2][1] * y + m0->m[2][2] * z) >>
                       12) + m0->t[2]);

    *m2 = tmp;
    return m2;
}

#ifdef MGS_ESPBOX
/* port/include/gtemac.h routes gte_CompMatrix() to this name */
MATRIX* psyz_CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2)
{
    return CompMatrix(m0, m1, m2);
}
#endif

/* Long-vector form of ApplyMatrix: absent from PSY-Z. */
VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1)
{
    long x = v0->vx, y = v0->vy, z = v0->vz;
    v1->vx = (m->m[0][0] * x + m->m[0][1] * y + m->m[0][2] * z) >> 12;
    v1->vy = (m->m[1][0] * x + m->m[1][1] * y + m->m[1][2] * z) >> 12;
    v1->vz = (m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z) >> 12;
    return v1;
}

/* PC* host-file stubs moved to psyq_compat.c */

/* --------------------------------------------------------------------------
 * The 804 KB of PSX main RAM the game lays its heaps out in.
 *
 * A plain array rather than a heap block, because RESIDENT_BOTTOM is used as a
 * static initializer in libgv/resident.c -- the address has to be a link-time
 * constant. EXT_RAM_BSS_ATTR puts it in PSRAM, which is right for the biggest
 * single reservation in the port: internal RAM is what the thread stacks need,
 * and this block is only ever data, never code and never a stack.
 * ------------------------------------------------------------------------ */

#include "esp_attr.h"

#ifdef MGS_ESPBOX
/* esp-box-emu: allocated from the emulator's 4MB ROM block while the game runs
 * (src/mgs.cpp); size MGS_MAIN_RAM_BYTES. */
unsigned char* mgs_main_ram;
#else
EXT_RAM_BSS_ATTR unsigned char mgs_main_ram[0x80000 + 0xC9000 + 0x20000];
#endif

/* Skeleton measurement switches, read by libdg/screen.c's DG_ApplyRots.
 * mgs_dbg_bindpose: force every joint angle to zero so the assembled pose is
 * a pure sum of the model's own offsets, checkable against the retail data.
 * mgs_dbg_skel:     how many skeletal objects still to dump. */
/* M1 answered: with the pose frozen, all sixteen joints landed exactly where
 * the retail data says, det was 4096 everywhere and every joint sat exactly
 * its own bone-length from its parent. The assembly chain is proven correct on
 * this board, so the deformation has to come from the ANGLES. M2 asks the same
 * three questions with the real animation running. */
int mgs_dbg_bindpose = 0;
int mgs_dbg_skel = 0;

/* Turntable angle, in the GTE's 4096-per-revolution units. Advanced once per
 * frame by libdg/display.c and read by libdg/screen.c when MGS_TURNTABLE is
 * defined; inert otherwise. */
int mgs_dbg_spin = 0;

/* How many level triggers have actually been crossed. Counted at the hit site
 * in libhzd/trap.c: the first version printed a local that nothing ever
 * incremented, so it reported zero all session and read as hard evidence that
 * the trigger system was dead. A statistic nobody increments is worse than no
 * statistic -- it invents a finding. */
int mgs_dbg_trap_hits = 0;

/* Somewhere for the codec's face group to live.
 *
 * The Colonel's calls draw from FACE.DAT, and the group the game asks for is
 * 417792 bytes -- measured on the board, and nearly three times what the
 * decompiled code's own comment suggests. GV_PACKET_MEMORY0 cannot find that,
 * so the allocation returned NULL, the loader read over a null pointer and the
 * parser dereferenced it: that crash is the whole reason the codec screen was
 * switched off and no conversation was ever seen.
 *
 * The PSX had no other pool to offer. This board does: 8 MB of PSRAM, of which
 * the game's own heaps use about 1.2. Half a megabyte reserved here is cheap,
 * and unlike a heap block it cannot fail at the worst moment. It is data the
 * renderer only reads, so PSRAM latency is the right trade.
 *
 * radiomes.c uses this as the fallback when the pool says no, and must not
 * hand it back to GV_FreeMemory -- freeing a static buffer would corrupt the
 * allocator's free list far away from here and long after. */
#ifdef MGS_ESPBOX
unsigned char* mgs_face_group;   /* esp-box-emu: from the ROM block, MGS_FACE_GROUP_BYTES */
const unsigned mgs_face_group_size = 0x80000;
#else
EXT_RAM_BSS_ATTR unsigned char mgs_face_group[0x80000];
const unsigned mgs_face_group_size = sizeof(mgs_face_group);
#endif

/* Who last blanked the screen. libdg/display.c prints it when DG_SwapFrame
 * takes its skip branch; the four sites that park DG_UnDrawFrameCount at
 * 0x7fff0000 stamp it. Guessing which one it was cost two flashes. */
const char *mgs_dbg_undraw_src = "(none)";

/* Where the sound task last got to.
 *
 * SOUND_INT spins and never yields, and its own printfs cannot say where it is
 * because the console drops them once the buffer fills. This is read and
 * printed by the vblank tick instead, whose one line per 3000 ticks does get
 * through -- a breadcrumb that survives a saturated console. */
const char *mgs_where = "(nada)";

#ifdef MGS_BOARD_XIAO
/* See port/mgs_printf.h for why this exists. Kept here rather than in a file
 * of its own because it must NOT have the printf macro applied to itself. */
#undef printf
#include <stdarg.h>
#include "driver/usb_serial_jtag.h"

int Mgs_Printf(const char* fmt, ...) {
    /* One shared buffer, and that is deliberate: this is called from several
     * mts tasks, but they are cooperative and run one at a time on core 0, so
     * they cannot interleave inside it. A per-call stack buffer would be
     * 512 bytes of the mts stacks, which are the scarcest memory here. */
    static char line[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n <= 0) return n;
    if (n > (int)sizeof line - 1) n = (int)sizeof line - 1;
    /* Zero timeout: write what fits, drop the rest, never wait. */
    usb_serial_jtag_write_bytes(line, n, 0);
    return n;
}
#endif
