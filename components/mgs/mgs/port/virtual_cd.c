/* A virtual CD over ordinary files.
 *
 * MGS's libfs does not open files by name. It reads the disc's ISO9660
 * directory once to learn which SECTOR each file starts at, and from then on
 * everything -- STAGE.DIR's stage table, the movie index, streamed audio --
 * addresses data as absolute sector numbers. There is no CD here and no ISO
 * directory, so that first read found garbage and took the board down.
 *
 * Rather than teach the whole of libfs about filenames, give it the disc it
 * expects: hand out a synthetic sector range per file, and translate every
 * sector read back into a seek on the corresponding file. Everything above
 * this layer then works unmodified, including the trimmed STAGE.DIR that
 * port/pack_stagedir.py builds -- its stage offsets are already sectors
 * relative to the start of the file, which is exactly what the game assumes.
 */

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "libfs/libfs.h"
#include "libfs/cdbios.h"

#define CD_SECTOR 2048

/* Ranges are spaced far enough apart that no real file can ever overlap the
 * next one; the retail disc's biggest file is DEMO.DAT at ~258 MB = 126k
 * sectors, so a million sectors per slot leaves plenty of headroom. */
#define SLOT_SECTORS 1000000u

static const char* const cd_names[FS_MAX_FILEID] = {
    "STAGE.DIR", "RADIO.DAT", "FACE.DAT", "ZMOVIE.STR",
    "VOX.DAT",   "DEMO.DAT",  "BRF.DAT",
};

/* Where the disc data is read from, and why it differs per medium.
 *
 * Flash partition: copied into PSRAM at boot. Reading sectors straight out of
 * it worked, but every read is a flash operation, and a flash operation on this
 * chip switches the instruction cache off on both cores. Any other core
 * executing from flash at that instant takes a cache fault -- which is what
 * killed the boot inside the SPU init, with the sound task running while a CD
 * read was in flight. Copying once, before a single game task exists, means
 * there is never a flash access again afterwards, and a sector read becomes a
 * memcpy. The cost is PSRAM: the partition holds at most 8 MB and the copy has
 * to fit alongside everything else the game allocates.
 *
 * microSD: left on the card and read on demand. The card is a SPI peripheral,
 * not the execute-in-place flash, so a read touches no cache and needs no copy
 * -- which lifts the size limit entirely. That is the only way the full 68.6 MB
 * STAGE.DIR, and with it every stage the game can ask for, fits at all.
 *
 * Whichever medium a file comes from, everything above this layer is unchanged:
 * cd_fetch serves a sector from the copy or from the file. */
static unsigned char* cd_data[FS_MAX_FILEID];
static FILE* cd_files[FS_MAX_FILEID];
static long cd_sizes[FS_MAX_FILEID];
/* where each file-backed slot's handle currently sits, so a sequential run
 * does not re-seek; -1 means unknown */
static long cd_pos[FS_MAX_FILEID];
static int cd_ready;

/* the platform layer knows where the data lives */
extern void Mgs_DataPath(const char* name, char* out, unsigned n);
extern const char* Mgs_GetDataRoot(void);

/* The card shares SPI2 with the panel and the driver cannot arbitrate the two
 * on its own -- see the note in esp32/main/lcd_esp32.c. Every access to a
 * file-backed slot is bracketed by this; a PSRAM-backed one is a memcpy and
 * needs nothing. */
extern void Mgs_SpiBusTake(void);
extern void Mgs_SpiBusGive(void);

/* the task structure the real CD BIOS drives; the callback contract is
 * written in terms of it, so the virtual drive fills in the same fields */
extern CDBIOS_TASK cd_bios_task_800B4E58;

void Mgs_CdInit(void) {
    int i;
    if (cd_ready) {
        return;
    }
    cd_ready = 1;
    /* the scanout task is already running by now, so even opening files has to
     * take the bus */
    Mgs_SpiBusTake();
    for (i = 0; i < FS_MAX_FILEID; i++) {
        char path[160];
        char devname[64];
        FILE* f;

        /* the card wins when it carries the file: it is both bigger and
         * cheaper to read from, so a board with a card ignores whatever was
         * packed into flash for the same name */
        /* esp-box-emu: the files live directly in the data root (/sdcard/mgs) */
        snprintf(path, sizeof(path), "%s/%s", Mgs_GetDataRoot(), cd_names[i]);
        f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            cd_sizes[i] = ftell(f);
            fseek(f, 0, SEEK_SET);
            cd_files[i] = f;
            cd_pos[i] = 0;
            printf("[vcd] %-10s %8ld bytes on microSD, sector %u\n",
                   cd_names[i], cd_sizes[i], (unsigned)(i * SLOT_SECTORS));
            continue;
        }

        snprintf(devname, sizeof(devname), "cdrom:\\MGS\\%s;1", cd_names[i]);
        Mgs_DataPath(devname, path, sizeof(path));
        f = fopen(path, "rb");
        if (!f) {
            cd_sizes[i] = 0;
            printf("[vcd] %-10s ABSENT (not on /sd or %s)\n", cd_names[i], path);
            continue;
        }
        fseek(f, 0, SEEK_END);
        cd_sizes[i] = ftell(f);
        fseek(f, 0, SEEK_SET);

        cd_data[i] = heap_caps_malloc((size_t)cd_sizes[i], MALLOC_CAP_SPIRAM);
        if (cd_data[i]) {
            size_t got = fread(cd_data[i], 1, (size_t)cd_sizes[i], f);
            fclose(f);
            printf("[vcd] %-10s %8ld bytes -> PSRAM, sector %u\n", cd_names[i],
                   (long)got, (unsigned)(i * SLOT_SECTORS));
        } else {
            /* no room: keep it on flash and accept the cache hazard for this
             * one -- still better than the file being missing outright */
            cd_files[i] = f;
            cd_pos[i] = 0;
            printf("[vcd] %-10s %8ld bytes stays on flash, sector %u\n",
                   cd_names[i], cd_sizes[i], (unsigned)(i * SLOT_SECTORS));
        }
    }
    Mgs_SpiBusGive();
}

static void cd_open_all(void) { Mgs_CdInit(); }

/* esp-box-emu: release everything Mgs_CdInit() opened so the game can be
 * started again later. */
void Mgs_CdDeinit(void) {
    int i;
    for (i = 0; i < FS_MAX_FILEID; i++) {
        if (cd_files[i]) { fclose(cd_files[i]); cd_files[i] = NULL; }
        if (cd_data[i]) { free(cd_data[i]); cd_data[i] = NULL; }
        cd_sizes[i] = 0;
        cd_pos[i] = 0;
    }
    cd_ready = 0;
}

/* Not every file on the retail disc is on the board -- ZMOVIE.STR alone is
 * 250 MB of full-motion video. Callers need to be able to ask, because libfs
 * happily parses whatever a missing file returns and acts on the result. */
int Mgs_CdFilePresent(int fileid) {
    cd_open_all();
    if (fileid < 0 || fileid >= FS_MAX_FILEID) {
        return 0;
    }
    return cd_data[fileid] != 0 || cd_files[fileid] != 0;
}

/* Build the position table libfs would otherwise read off the disc. */
int FS_CdMakePositionTable(char* buffer, FS_FILE_INFO* finfo) {
    int i;
    (void)buffer;
    cd_open_all();
    for (i = 0; i < FS_MAX_FILEID; i++) {
        finfo[i].name = cd_names[i];
        finfo[i].pos = (u_int)(i * SLOT_SECTORS);
    }
    /* disc 1; the game uses this to pick its executable name */
    return 0;
}

/* --------------------------------------------------------------------------
 * The CD BIOS itself
 * ------------------------------------------------------------------------ */

static int cd_last_result;

int CDBIOS_Reset(void) {
    cd_open_all();
    return 0;
}

/* Copy one sector's worth of words out of the virtual disc.
 * Returns 0 when the request falls past the end of the file. */
static int cd_fetch(void* dst, unsigned sector, unsigned words) {
    unsigned slot = sector / SLOT_SECTORS;
    long offset = (long)(sector % SLOT_SECTORS) * CD_SECTOR;
    unsigned bytes = words * 4u;

    if (slot >= FS_MAX_FILEID || offset >= cd_sizes[slot]) {
        return 0;
    }
    if (offset + (long)bytes > cd_sizes[slot]) {
        unsigned have = (unsigned)(cd_sizes[slot] - offset);
        memset((char*)dst + have, 0, bytes - have);
        bytes = have;
    }
    if (cd_data[slot]) {
        memcpy(dst, cd_data[slot] + offset, bytes);
    } else if (cd_files[slot]) {
        int ok;
        Mgs_SpiBusTake();
        /* Seek only when the read is not already where we left off.
         *
         * A stage load is one long sequential run of ~550 sectors, and FATFS
         * resolves a byte offset by walking the file's cluster chain -- on a
         * 68 MB STAGE.DIR that is a long walk, repeated for every sector, and
         * it dominated the load: 1.1 MB took 11.5 s, about 95 KB/s, when the
         * bus itself can do more than ten times that. Tracking the position
         * and skipping the redundant seek costs one long per file. */
        ok = 1;
        if (cd_pos[slot] != offset) {
            ok = fseek(cd_files[slot], offset, SEEK_SET) == 0;
            cd_pos[slot] = ok ? offset : -1;
        }
        if (ok) {
            ok = fread(dst, 1, bytes, cd_files[slot]) == bytes;
            cd_pos[slot] = ok ? offset + (long)bytes : -1;
        }
        Mgs_SpiBusGive();
        if (!ok) {
            return 0;
        }
    } else {
        return 0;
    }
    return 1;
}

/* The real CD BIOS is asynchronous, and that is LOAD-BEARING, not an
 * implementation detail. The stage loader is a pair of co-routines: the drive
 * delivers sectors into a shared window while the FS daemon parses and copies
 * them out between deliveries, and when a file finishes, its callback points
 * the SAME window at the next file. Delivering the whole 275-sector stage in
 * one synchronous burst let file 2 overwrite file 1 before the parser had seen
 * a byte of it -- the "ntag size 0" garbage was our own second file on top of
 * the first.
 *
 * So: ReadRequest only records the request, and CDBIOS_ReadSync -- the exact
 * point where libfs polls "has more data arrived?" -- delivers it. One burst
 * per poll, stopped early whenever the callback returns 2, because that is the
 * "I redirected the buffer" signal: the parser must be given the CPU before
 * anything lands on the redirected window. Callbacks also rewrite
 * task->size/remaining mid-flight (that is how the config sector extends the
 * read to the whole stage block), so both are re-read every iteration. Sizes
 * are in 32-bit words, 512 to a sector, the way the task structure counts. */
void CDBIOS_ReadRequest(void* buffer, unsigned int sector, unsigned int size,
                        void* callback) {
    CDBIOS_TASK* task = &cd_bios_task_800B4E58;

    task->buffer = buffer;
    if (size == 0) {
        size = 0x7fff0000; /* "read until the callback says stop" */
    }
    task->remaining = (int)((size + 3) >> 2);
    task->size = (int)((size + 3) >> 2);
    task->sector = (int)sector;
    task->callback = (cdbios_task_pfn)callback;
    task->sectors_delivered = 0;
    task->ticks = 0;
    task->state = CDBIOS_STATE_READ;
    cd_last_result = 0;
}

/* deliver sectors for the outstanding request; returns 1 while incomplete */
static int cd_pump(void) {
    CDBIOS_TASK* task = &cd_bios_task_800B4E58;
    int burst;

    static int pump_log = 40;
    if (task->state != CDBIOS_STATE_READ) {
        return 0;
    }

    /* A sector out of PSRAM is a memcpy, so the whole request can be delivered
     * in one poll and the early-stops below do the real pacing. A sector off
     * the card is a real SPI transfer, and this runs on the loader actor, so a
     * big burst stalls the frame loop.
     *
     * That stall was worth avoiding when the concern was a hitch mid-game, but
     * a stage load is not mid-game: the screen is black and nothing is being
     * drawn until it finishes. Pacing it at 16 sectors a frame just made the
     * black screen last longer -- the heliport took 12 seconds, which reads as
     * a hang rather than a load. Take bigger bites; the only thing being kept
     * responsive during a load is a picture nobody can see. */
    {
        unsigned slot = (unsigned)task->sector / SLOT_SECTORS;
        int from_file = slot < FS_MAX_FILEID && cd_data[slot] == 0;
        burst = from_file ? 128 : 512;
    }
    if (pump_log > 0) {
        pump_log--;
        printf("[vcd] pump sector %d remaining %d buf %p\n", task->sector,
               task->remaining, task->buffer);
    }
    while (task->remaining > 0 && burst-- > 0) {
        unsigned words = task->remaining <= 512 ? (unsigned)task->remaining : 512u;

        if (task->buffer && !cd_fetch(task->buffer, (unsigned)task->sector, words)) {
            task->state = CDBIOS_STATE_IDLE; /* off the end of the file */
            return 0;
        }
        task->remaining -= (int)words;
        task->buffer_size = (int)words;

        if (task->callback) {
            int status = task->callback(task);
            if (status == 0) {
                task->state = CDBIOS_STATE_IDLE; /* callback ended the read */
                return 0;
            }
            if (status == 2) {
                /* buffer redirected: let the parser run before more arrives */
                task->sector++;
                task->sectors_delivered++;
                return 1;
            }
        }
        if (task->buffer) {
            task->buffer = (int*)task->buffer + words;
        }
        task->sector++;
        task->sectors_delivered++;
    }
    if (task->remaining <= 0) {
        task->state = CDBIOS_STATE_IDLE;
        return 0;
    }
    return 1;
}

int CDBIOS_ReadSync(void) {
    return cd_pump();
}
