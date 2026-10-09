/* The platform layer PSY-Z expects the host to supply, in its headless form.
 *
 * These are the same seventeen hooks the SOTN port implements in
 * esp32/main/esp_shim.c and esp_fileapi.c; this file is the MGS equivalent,
 * written so a link succeeds and a boot can be traced over the console before
 * any display, pad or audio hardware is wired up. Swapping in the real ESP-IDF
 * implementations later is a file substitution, not a redesign.
 *
 * Deliberately NOT included here: anything that touches the LCD, the GPIO
 * buttons or I2S. Those belong in an esp32/ component next to this one, so the
 * portable core stays buildable on a PC for A/B comparison.
 */

#include <psyz.h>
#include <libgpu.h>
#include <psyz/log.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------------------
 * Video: the vsync hook PSY-Z's VSync() sits on
 *
 * Returns elapsed time in units of the PSX's ~16.7 ms field, which is what the
 * game's frame pacing reads. Headless, so it just counts.
 * ------------------------------------------------------------------------ */

unsigned int mgs_frame_count;   /* readable from a debugger / telemetry */

int Psyz_VideoVSync(int mode) {
    if (mode < 0) {
        /* VSync(-1) asks how many vertical blanks have elapsed since boot. On
         * the PSX that counter is driven by the hardware and runs whether or
         * not the game is drawing -- mts_VSyncCallback reads it to decide
         * which sleeping tasks are due. Returning the number of PRESENTED
         * frames instead means the clock never moves when nothing is being
         * drawn, so every mts_wait_vbl() sleeps forever: the boot stops dead
         * at the first daemon that waits for a frame. */
        extern unsigned mgs_vblank_count;
        return (int)mgs_vblank_count;
    }
    if (mode == 0) {
        mgs_frame_count++;
        if ((mgs_frame_count % 60u) == 1u) {
            extern DISPENV g_dispenv;
            printf("[vsync] frame %u  disp %d,%d %dx%d\n", mgs_frame_count,
                   g_dispenv.disp.x, g_dispenv.disp.y,
                   g_dispenv.disp.w, g_dispenv.disp.h);
        }
    }
    return (int)mgs_frame_count;
}

/* --------------------------------------------------------------------------
 * Pad
 * ------------------------------------------------------------------------ */

/* Filled by whatever input source exists: serial keys during bring-up, GPIO
 * buttons on the board. Zero means "nothing pressed", which is what a headless
 * boot wants. */
unsigned short mgs_pad_state[2];

void Psyz_PadsPoll(void) {
    /* no input source yet; the pad buffers keep whatever mgs_pad_state holds */
}

/* --------------------------------------------------------------------------
 * Audio
 * ------------------------------------------------------------------------ */

int Psyz_AudioInit(void) {
    return 0; /* no mixer yet; the SPU emulation runs but produces no output */
}

void Psyz_AudioLock(void) {}
void Psyz_AudioUnlock(void) {}

/* --------------------------------------------------------------------------
 * Logging
 * ------------------------------------------------------------------------ */

LOG_LEVEL psyz_logLevel = LOG_LEVEL_W;

void psyz_log(unsigned int level, const char* file, unsigned int line,
              const char* func, const char* fmt, ...) {
    static const char tag[] = "DIWE";
    va_list ap;
    (void)file;
    printf("[%c] %s:%u: ", level < sizeof(tag) - 1 ? tag[level] : '?',
           func ? func : "?", line);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

/* --------------------------------------------------------------------------
 * Low-level file API
 *
 * PSY-Z macro-renames open/read/write/lseek/close, which is why these live in
 * their own translation unit in the SOTN port: any file that sees those macros
 * gets both the libc declarations and the wrapper bodies rewritten. Here the
 * bodies are self-contained, so there is nothing to knot.
 * ------------------------------------------------------------------------ */

/* The game asks for "cdrom:\MGS\NAME;1"; map that onto a plain directory so a
 * PC run and a microSD run take the same path. */
static const char* mgs_data_root = ".";

void Mgs_SetDataRoot(const char* path) {
    if (path) mgs_data_root = path;
}
const char* Mgs_GetDataRoot(void) { return mgs_data_root; }

void Mgs_DataPath(const char* devname, char* out, unsigned n);

static void translate(const char* devname, char* out, size_t n) {
    const char* p = devname;
    size_t i = 0;
    if (!p) {
        out[0] = 0;
        return;
    }
    /* strip the "cdrom:" prefix and the ";1" version suffix, and turn the
     * PSX's backslashes into ordinary separators */
    if (!strncmp(p, "cdrom:", 6)) p += 6;
    if (n == 0) return;
    i = (size_t)snprintf(out, n, "%s/", mgs_data_root);
    if (i >= n) {
        /* the data root alone does not fit: snprintf returned the length it
         * wanted, not what it wrote, so it is not an index into out */
        out[n - 1] = 0;
        return;
    }
    for (; *p && i + 1 < n; p++) {
        if (*p == ';') break;
        out[i++] = (*p == '\\') ? '/' : *p;
    }
    out[i] = 0;
}

/* the virtual CD needs the same cdrom: -> /flash mapping */
void Mgs_DataPath(const char* devname, char* out, unsigned n) {
    translate(devname, out, (size_t)n);
}

int psyz_open(const char* devname, int flag) {
    char path[256];
    FILE* f;
    translate(devname, path, sizeof(path));
    f = fopen(path, (flag & 1) ? "r+b" : "rb");
    if (!f) {
        printf("[fs] open failed: %s (from %s)\n", path,
               devname ? devname : "(null)");
        return -1;
    }
    return (int)(size_t)f;
}

int psyz_close(int fd) {
    if (fd <= 0) return -1;
    return fclose((FILE*)(size_t)fd);
}

long psyz_read(long fd, void* buf, long n) {
    if (fd <= 0) return -1;
    return (long)fread(buf, 1, (size_t)n, (FILE*)(size_t)fd);
}

long psyz_write(long fd, void* buf, long n) {
    if (fd <= 0) return -1;
    return (long)fwrite(buf, 1, (size_t)n, (FILE*)(size_t)fd);
}

long psyz_ioctl(long fd, long com, long arg) {
    (void)fd; (void)com; (void)arg;
    return 0;
}

long psyz_lseek(long fd, long offset, long flag) {
    if (fd <= 0) return -1;
    if (fseek((FILE*)(size_t)fd, offset, (int)flag) != 0) return -1;
    return ftell((FILE*)(size_t)fd);
}

/* --------------------------------------------------------------------------
 * Memory-card style file helpers
 * ------------------------------------------------------------------------ */

struct DIRENTRY;

struct DIRENTRY* my_firstfile(const char* pattern, struct DIRENTRY* entry) {
    (void)pattern;
    (void)entry;
    return 0; /* no card enumeration yet */
}

struct DIRENTRY* my_nextfile(struct DIRENTRY* entry) {
    (void)entry;
    return 0;
}

long my_erase(const char* path) {
    char full[256];
    translate(path, full, sizeof(full));
    return remove(full) == 0 ? 1 : 0;
}

long my_format(const char* fs) {
    (void)fs;
    return 1;
}

/* Budget for mts assertions: see the note in source/mts/mts_new.h. Enough to
 * see the first failures, few enough to leave the boot log readable. */
int mts_assert_budget = 12;
