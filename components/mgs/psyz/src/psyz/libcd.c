#include <psyz.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define PSYZ_ESP_XRAM EXT_RAM_BSS_ATTR
#else
#define PSYZ_ESP_XRAM
#endif
#include <libetc.h>
#include <libcd.h>
#include <psyz/log.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "../internal.h"
#include "libspu/libspu_private.h" /* esp-box-emu: psyq/src on the include path */

#define SECTOR_SIZE 2352
#define MAX_TRACKS 99
#define MAX_PATH_LEN 512

// Track types
typedef enum {
    TRACK_AUDIO,
    TRACK_MODE1_2352,
    TRACK_MODE2_2352,
    TRACK_UNKNOWN
} TrackType;

// Track entry from CUE file
typedef struct {
    char file_path[MAX_PATH_LEN];
    int start_sector; // MSF sector offset within the file
    int abs_sector;   // Absolute sector position on the "virtual CD"
    int track_num;
    TrackType type;
    int is_valid;
} TrackEntry;

// https://www.problemkaputt.de/psx-spx.htm#cdromcontrollerioports
typedef struct {
    u_char cd_src_l_dst_l;  // volume for CD(L) -> SPU (L)
    u_char cd_src_l_dst_r;  // volume for CD(L) -> SPU (R)
    u_char cd_src_r_dst_l;  // volume for CD(R) -> SPU (L)
    u_char cd_src_r_dst_r;  // volume for CD(R) -> SPU (R)
    u_char adpcm_mute;      // 0=Normal, 1=Mute
    u_char is_stereo;       // 0=Mono, 1=Stereo
    u_char sample_rate;     // 0=37800Hz, 1=18900Hz
    u_char bits_per_sample; // 0=4bit, 1=8bit
    u_char emphasis;        // 0=Off, 1=Emphasis
} CdContext;

// Global CUE data storage
PSYZ_ESP_XRAM static TrackEntry g_tracks[MAX_TRACKS];
static int g_track_count = 0;
static char g_cue_base_path[MAX_PATH_LEN];

char* CD_comstr[] = {
    "CdlSync",    "CdlNop",
    "CdlSetloc",  "CdlPlay",
    "CdlForward", "CdlBackward",
    "CdlReadN",   "CdlStandby",
    "CdlStop",    "CdlPause",
    "CdlReset",   "CdlMute",
    "CdlDemute",  "CdlSetfilter",
    "CdlSetmode", "CdlGetparam",
    "CdlGetlocL", "CdlGetlocP",
    "?",          "CdlGetTN",
    "CdlGetTD",   "CdlSeekL",
    "CdlSeekP",   "?",
    "?",          "?",
    "?",          "CdlReadS",
    "?",          "?",
    "?",          "?",
};
char* CD_intstr[] = {
    "NoIntr",  "DataReady", "Complete", "Acknowledge",
    "DataEnd", "DiskError", "?",        "?",
};

CdlCB CD_cbsync = NULL;
CdlCB CD_cbready = NULL;
int CD_cbread = 0;
int CD_debug = 0;
int CD_status = 0;
int CD_status1 = 0;
int CD_nopen = 0;
CdlLOC CD_pos = {2, 0, 0, 0};
u_char CD_mode = 0;
u_char CD_com = 0;
int DS_active = 0;
static CdContext ctx;

int CD_cw(u8 com, u8* param, u_char* result, s32 arg3);
int CD_sync(int mode, u_char* result);

static void ADPMUTE(int mute) {
    // 0x1F801800 = 3
    ctx.adpcm_mute = mute != 0; // 0x1F801803 = mute & 1
}

static int enable_passthrough = 1;
static void CHNGATV() {
    // 0x1F801800 = 3
    // 0x1F801803 = 0x20

    // enabling passthrough avoid the transformation of the CD pulled samples
    // to be re-mixed with different volume values, leading an increase of
    // performance on weak/embedded targets.
    //
    // PS1 defaults CD_vol as 127,0,127,0 - if detected, enable passthrough.
    //
    // Volume mix algo on PsyZ is (sample * volume) / 128, meaning 128,0,128,0
    // would also return 1:1 samples - enable passthrough here too.
    //
    // volume at 127 and 128 would theoretically generate sound at different
    // levels of volume, but no normal human would ever notice such difference.
    enable_passthrough =
        (ctx.cd_src_l_dst_l == 127 || ctx.cd_src_l_dst_l == 128) &&
        (ctx.cd_src_r_dst_r == 127 || ctx.cd_src_r_dst_r == 128) &&
        ctx.cd_src_l_dst_r == 0 && ctx.cd_src_r_dst_l == 0;
}

static short clamp16(int v) {
    if (v < -32768)
        return -32768;
    if (v > 32767)
        return 32767;
    return (short)v;
}

// Trim whitespace from both ends of a string
static char* str_trim(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == 0) {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    return str;
}

// Extract quoted string or unquoted token
static char* extract_token(char* str, char* dest, int max_len) {
    char* p = str;
    int len = 0;

    // Skip leading whitespace
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p == '"') {
        // Quoted string
        p++;
        while (*p && *p != '"' && len < max_len - 1) {
            dest[len++] = *p++;
        }
        if (*p == '"') {
            p++;
        }
    } else {
        // Unquoted token (up to space)
        while (*p && !isspace((unsigned char)*p) && len < max_len - 1) {
            dest[len++] = *p++;
        }
    }
    dest[len] = '\0';
    return p;
}

// Get file size in sectors
static int get_file_size_in_sectors(const char* file_path) {
    FILE* fp = fopen(file_path, "rb");
    if (!fp) {
        WARNF("Failed to open file for size calculation: %s", file_path);
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fclose(fp);

    if (file_size < 0) {
        WARNF("Failed to get file size: %s", file_path);
        return 0;
    }

    // Convert bytes to sectors (rounded up)
    return (int)((file_size + SECTOR_SIZE - 1) / SECTOR_SIZE);
}

// Parse a CUE file and populate the global track table
static int parse_cue_file(const char* cue_path) {
    FILE* fp = fopen(cue_path, "r");
    if (!fp) {
        ERRORF("error opening CUE file %s", cue_path);
        return -1;
    }

    // Extract base directory from CUE path for resolving relative file paths
    const char* last_slash = strrchr(cue_path, '/');
    if (!last_slash) {
        last_slash = strrchr(cue_path, '\\');
    }
    if (last_slash) {
        unsigned int len = (unsigned int)(last_slash - cue_path + 1);
        if (len >= MAX_PATH_LEN) {
            len = MAX_PATH_LEN - 1;
        }
        strncpy(g_cue_base_path, cue_path, len);
        g_cue_base_path[len] = '\0';
    } else {
        g_cue_base_path[0] = '\0';
    }

    char full_line[1024];
    char current_file[MAX_PATH_LEN] = {0};
    char current_file_full_path[MAX_PATH_LEN] = {0};
    int current_track = -1;
    int current_file_abs_sector =
        0; // Absolute sector where current file starts

    g_track_count = 0;
    while (fgets(full_line, sizeof(full_line), fp)) {
        char* line = str_trim(full_line);
        if (strncmp(line, "FILE", 4) == 0) {
            // Before switching to new file, calculate size of previous file
            if (current_file[0] != '\0') {
                int prev_file_size =
                    get_file_size_in_sectors(current_file_full_path);
                current_file_abs_sector += prev_file_size;
            }

            char* p = line + 4;
            extract_token(p, current_file, MAX_PATH_LEN);

            // Build full path for the new file
            if (current_file[0] == '/' || current_file[0] == '\\' ||
                (current_file[0] != '\0' && current_file[1] == ':')) {
                // Absolute path
                strncpy(current_file_full_path, current_file, MAX_PATH_LEN - 1);
            } else {
                // Relative path - prepend base directory
                snprintf(current_file_full_path, MAX_PATH_LEN, "%s%s",
                         g_cue_base_path, current_file);
            }
        } else if (strncmp(line, "TRACK", 5) == 0) {
            int track_num = 1;
            char type_str[64];
            if (sscanf(line, "TRACK %d %63s", &track_num, type_str) == 2) {
                if (track_num < 1 || track_num > MAX_TRACKS) {
                    ERRORF("Invalid track number: %d", track_num);
                    continue;
                }

                current_track = track_num - 1;
                TrackEntry* track = &g_tracks[current_track];
                track->track_num = track_num;
                track->is_valid = 1;
                track->start_sector = 0; // Will be set by INDEX 01
                track->abs_sector =
                    0; // Will be calculated when INDEX 01 is parsed

                // Store file path
                strncpy(
                    track->file_path, current_file_full_path, MAX_PATH_LEN - 1);

                if (strcmp(type_str, "AUDIO") == 0) {
                    track->type = TRACK_AUDIO;
                } else if (strcmp(type_str, "MODE1/2352") == 0) {
                    track->type = TRACK_MODE1_2352;
                } else if (strcmp(type_str, "MODE2/2352") == 0) {
                    track->type = TRACK_MODE2_2352;
                } else {
                    track->type = TRACK_UNKNOWN;
                    DEBUGF("Unknown track type: %s", type_str);
                }

                if (track_num > g_track_count) {
                    g_track_count = track_num;
                }
            }
        } else if (strncmp(line, "INDEX", 5) == 0) {
            if (current_track < 0) {
                ERRORF("INDEX found before TRACK");
                continue;
            }
            int index_num;
            char msf_str[32];
            if (sscanf(line, "INDEX %d %31s", &index_num, msf_str) == 2) {
                int minute = 0, second = 0, frame = 0;
                if (sscanf(msf_str, "%d:%d:%d", &minute, &second, &frame) ==
                    3) {
                    int sector = minute * 60 * 75 + second * 75 + frame;
                    if (index_num == 1) { // INDEX 01 is the track data
                        TrackEntry* track = &g_tracks[current_track];
                        track->start_sector = sector;
                        track->abs_sector = current_file_abs_sector + sector;
                        DEBUGF("Track %02d: %s at sector %d", track->track_num,
                               track->file_path, track->abs_sector);
                    }
                }
            }
        } else {
            WARNF("CUE line not recognized: %s", line);
        }
    }
    fclose(fp);

    if (g_track_count == 0) {
        ERRORF("No valid tracks found in CUE file");
        return -1;
    }
    DEBUGF("Parsed CUE file: %d tracks", g_track_count);
    return 0;
}

// Cached last-command response, exposed via CD_sync().
static u_char last_result[8];
static int last_intr = CdlComplete;

static void cache_last_result(const u_char* result, int intr) {
    memcpy(last_result, result, sizeof(last_result));
    last_intr = intr;
}

static int is_disk_loaded = 0;

// Compute the absolute sector of the disc lead-out, used by CdlGetTD with
// param = 0x00. Equals the absolute sector where the last track's file ends.
static int lead_out_sector(void) {
    if (g_track_count == 0) {
        return 0;
    }
    TrackEntry* last = &g_tracks[g_track_count - 1];
    return last->abs_sector + get_file_size_in_sectors(last->file_path);
}

// 1 = disc lid is open, 0 = lid closed
static int shell_is_open = 0;
void Psyz_CdShellOpen(int is_open) {
    shell_is_open = is_open ? 1 : 0;
    if (shell_is_open) {
        CD_status |= CdlStatShellOpen;
        CD_status &= ~CdlStatStandby;
    } else {
        CD_status &= ~CdlStatShellOpen;
        CD_status |= CdlStatStandby;
    }
}

int Psyz_CdSetDiskPath(const char* diskPath) {
    is_disk_loaded = 0;
    g_track_count = 0;
    memset(g_tracks, 0, sizeof(g_tracks));
    if (!diskPath) {
        DEBUGF("Disk path unset");
        return 0;
    }
    if (parse_cue_file(diskPath) < 0) {
        return -1;
    }
    is_disk_loaded = 1;
    DEBUGF("Disk path set: %s", diskPath);
    return 0;
}

static PsyzCdReadCB disk_read_cb = NULL;
void Psyz_CdSetReadCB(PsyzCdReadCB cb) { disk_read_cb = cb; }

#define N_CHANNELS 2
#define SAMPLE_SIZE sizeof(short)
#define SECTOR_SIZE 2352
#define BUFFER_SECTORS 8 // Read 8 sectors at a time
#define CD_BUF_FRAMES                                                          \
    (SECTOR_SIZE * BUFFER_SECTORS / (N_CHANNELS * SAMPLE_SIZE))
static FILE* track_file;
PSYZ_ESP_XRAM static s16 cd_buf[CD_BUF_FRAMES * N_CHANNELS];
static size_t cd_buf_pos = 0;   // current read position (in frames)
static size_t cd_buf_count = 0; // number of valid frames in buffer
static int is_playing = 0; // pause or unpause seeking through the CD stream
static int is_muted = 0;   // return empty samples while CD keeps streaming

// CD audio pull callback — called by SPU when its internal ring buffer
// runs low. Fills `buf` with up to `max_frames` interleaved stereo frames.
// One mutex lock per batch rather than per frame.
static size_t cdda_pull_samples(short* buf, size_t max_frames) {
    size_t written = 0;
    int hit_eof = 0;
    Psyz_AudioLock();
    if (!is_playing || !track_file) {
        goto end;
    }
    while (written < max_frames) {
        // Refill file read buffer if consumed
        if (cd_buf_pos >= cd_buf_count) {
            size_t nRead = fread(cd_buf, 1, sizeof(cd_buf), track_file);
            if (nRead > 0) {
                cd_buf_count = (int)(nRead / (N_CHANNELS * SAMPLE_SIZE));
                cd_buf_pos = 0;
            } else {
                DEBUGF("cd audio playback reached end of file");
                is_playing = 0;
                hit_eof = 1;
                break;
            }
        }
        // Copy as many frames as possible from file buffer to output
        const size_t avail = cd_buf_count - cd_buf_pos;
        const size_t want = max_frames - written;
        const size_t n = avail < want ? avail : want;
        if (is_muted) {
            memset(&buf[written * 2], 0, n * N_CHANNELS * SAMPLE_SIZE);
        } else {
            memcpy(&buf[written * 2], &cd_buf[cd_buf_pos * 2],
                   n * N_CHANNELS * SAMPLE_SIZE);
        }
        cd_buf_pos += n;
        written += n;
    }
end:
    Psyz_AudioUnlock();
    if (hit_eof && CD_cbready) {
        CD_cbready(CdlDataEnd, NULL);
    }
    return written;
}

#define XA_DECODED_MAX_FRAMES 4032            // 18 blocks * 4 sub * 28 samples
#define XA_STEP_Q16 ((37800u << 16) / 44100u) // 56173

PSYZ_ESP_XRAM static struct {
    short decoded[XA_DECODED_MAX_FRAMES * 2];
    int active;
    unsigned char filter_file;
    unsigned char filter_channel;
    int filter_set; // 1 once CdlSetfilter has been called
    int decoded_count;
    int decoded_pos;
    int hist_l_old, hist_l_older;
    int hist_r_old, hist_r_older;
    // Hermite ring: index [0]=oldest .. [3]=newest per channel
    short rh_l[4], rh_r[4];
    unsigned int phase; // 16.16 fractional position into the input stream
    int cur_abs_sector; // absolute last sector used, for CdlGetlocL
} xa;

static void xa_reset_stream(void) {
    xa.decoded_count = 0;
    xa.decoded_pos = 0;
    xa.hist_l_old = xa.hist_l_older = 0;
    xa.hist_r_old = xa.hist_r_older = 0;
    memset(xa.rh_l, 0, sizeof(xa.rh_l));
    memset(xa.rh_r, 0, sizeof(xa.rh_r));
    xa.phase = 0;
}

static int xa_sector_matches(unsigned char file, unsigned char channel) {
    if (!xa.filter_set) {
        return 1;
    }
    if (file != xa.filter_file) {
        return 0;
    }
    if (xa.filter_channel == 0) {
        return 1;
    }
    if (channel == xa.filter_channel) {
        return 1;
    }
    return 0;
}

static int s4(int n) { return (n & 0x8) ? (n - 0x10) : n; }

static const int xa_pos[4] = {0, 60, 115, 98};
static const int xa_neg[4] = {0, 0, -52, -55};
// Decode 28 samples for one (block, nibble) pair into `dst` (stride=2 stereo
// or stride=1 mono). Returns updated old/older via pointers.
static void xa_decode_28(const unsigned char* blk, int sub, int nibble,
                         short* dst, int stride, int* prev, int* prev2) {
    const unsigned char hdr = blk[4 + sub * 2 + nibble];
    const int shift_in = hdr & 0x0F;
    const int shift = (shift_in > 12) ? 9 : (12 - shift_in);
    const int filter = (hdr & 0x30) >> 4; // 0..3
    const int f0 = xa_pos[filter];
    const int f1 = xa_neg[filter];
    for (int j = 0; j < 28; j++) {
        const unsigned char byte = blk[16 + sub + j * 4];
        const int t = s4((byte >> (nibble * 4)) & 0x0F);
        const int s = (t << shift) + (((*prev) * f0 + (*prev2) * f1 + 32) / 64);
        dst[j * stride] = clamp16(s);
        *prev2 = *prev;
        *prev = s;
    }
}

// Decode a Form2 user-data area (0x914 = 2324 bytes) of 4-bit stereo XA.
// Writes interleaved L,R s16 frames into xa.decoded; returns frame count.
static int xa_decode_user_stereo_4bit(const unsigned char* user) {
    int frame = 0;
    for (int i = 0; i < 18; i++) {
        const unsigned char* block = user + i * 128;
        for (int sub = 0; sub < 4; sub++) {
            xa_decode_28(block, sub, 0, &xa.decoded[(frame) * 2 + 0], 2,
                         &xa.hist_l_old, &xa.hist_l_older);
            xa_decode_28(block, sub, 1, &xa.decoded[(frame) * 2 + 1], 2,
                         &xa.hist_r_old, &xa.hist_r_older);
            frame += 28;
        }
    }
    return frame;
}

// Decode a Form2 user-data area of 4-bit mono XA: both nibble passes write
// to the same mono buffer; we duplicate to L and R for output.
static int xa_decode_user_mono_4bit(const unsigned char* user) {
    short mono[XA_DECODED_MAX_FRAMES];
    int frame = 0;
    for (int i = 0; i < 18; i++) {
        const unsigned char* block = user + i * 128;
        for (int sub = 0; sub < 4; sub++) {
            xa_decode_28(block, sub, 0, &mono[frame], 1, &xa.hist_l_old,
                         &xa.hist_l_older);
            frame += 28;
            xa_decode_28(block, sub, 1, &mono[frame], 1, &xa.hist_l_old,
                         &xa.hist_l_older);
            frame += 28;
        }
    }
    for (int i = 0; i < frame; i++) {
        xa.decoded[i * 2 + 0] = mono[i];
        xa.decoded[i * 2 + 1] = mono[i];
    }
    return frame;
}

static int xa_read_and_decode_sector(void) {
    unsigned char sector[SECTOR_SIZE];
    while (1) {
        const size_t n = fread(sector, 1, SECTOR_SIZE, track_file);
        if (n != SECTOR_SIZE) {
            return 0; // EOF or short read
        }
        xa.cur_abs_sector++;
        const unsigned char file = sector[0x10];
        const unsigned char channel = sector[0x11];
        const unsigned char submode = sector[0x12];
        const unsigned char ci = sector[0x13];
        if ((submode & 0x44) != 0x44) {
            continue; // data sector, skip from XA stream
        }
        if (!xa_sector_matches(file, channel)) {
            continue;
        }
        const unsigned char* user = &sector[0x18];
        int stereo = (ci & 0x03) == 1;
        int rate18900 = (ci & 0x04) != 0;
        int bits8 = (ci & 0x30) != 0;
        if (rate18900 || bits8) {
            LOG_ONCE(
                "XA: 18900Hz or 8-bit not supported, decoding as 4bit/37800");
        }
        if (stereo) {
            xa.decoded_count = xa_decode_user_stereo_4bit(user);
        } else {
            xa.decoded_count = xa_decode_user_mono_4bit(user);
        }
        xa.decoded_pos = 0;
        return 1;
    }
}

// Pull next 37800 Hz stereo input frame into the Hermite ring buffers.
// Returns 1 on success, 0 if stream ended.
static int xa_advance_input(void) {
    if (xa.decoded_pos >= xa.decoded_count) {
        if (!xa_read_and_decode_sector()) {
            return 0;
        }
    }
    short l = xa.decoded[xa.decoded_pos * 2 + 0];
    short r = xa.decoded[xa.decoded_pos * 2 + 1];
    xa.decoded_pos++;
    xa.rh_l[0] = xa.rh_l[1];
    xa.rh_l[1] = xa.rh_l[2];
    xa.rh_l[2] = xa.rh_l[3];
    xa.rh_l[3] = l;
    xa.rh_r[0] = xa.rh_r[1];
    xa.rh_r[1] = xa.rh_r[2];
    xa.rh_r[2] = xa.rh_r[3];
    xa.rh_r[3] = r;
    return 1;
}

// 4-point Hermite (Catmull-Rom-like) at fractional position frac in [0,1).
// frac is given as Q1.16 (only low 16 bits used).
// TODO !!! slown down considerably on hardware without float processing unit
static short hermite4(
    short ym1, short y0, short y1, short y2, unsigned int frac_q16) {
    // Use floating math here for correctness; this is run-once per output
    // sample and is still very cheap. Embedded targets get clean output.
    float x = (float)(frac_q16 & 0xFFFF) / 65536.0f;
    float c0 = (float)y0;
    float c1 = 0.5f * (float)(y1 - ym1);
    float c2 =
        (float)ym1 - 2.5f * (float)y0 + 2.0f * (float)y1 - 0.5f * (float)y2;
    float c3 = 0.5f * (float)(y2 - ym1) + 1.5f * (float)(y0 - y1);
    float v = ((c3 * x + c2) * x + c1) * x + c0;
    int iv = (int)(v + (v >= 0 ? 0.5f : -0.5f));
    return clamp16(iv);
}

static size_t xa_pull_samples(short* buf, size_t max_frames) {
    size_t written = 0;
    int hit_eof = 0;
    Psyz_AudioLock();
    if (!is_playing || !track_file || !xa.active) {
        goto end;
    }
    // Prime Hermite ring on first call so we have y0..y2 valid.
    while (xa.rh_l[3] == 0 && xa.rh_l[2] == 0 && xa.rh_l[1] == 0 &&
           xa.decoded_pos == 0 && xa.decoded_count == 0) {
        if (!xa_advance_input()) {
            hit_eof = 1;
            goto end;
        }
        if (xa.decoded_count > 0)
            break;
    }
    while (written < max_frames) {
        // Advance input as many times as needed to get phase < 1.0.
        while (xa.phase >= 0x10000) {
            if (!xa_advance_input()) {
                hit_eof = 1;
                goto end;
            }
            xa.phase -= 0x10000;
        }
        if (is_muted) {
            buf[written * 2 + 0] = 0;
            buf[written * 2 + 1] = 0;
        } else {
            buf[written * 2 + 0] = hermite4(
                xa.rh_l[0], xa.rh_l[1], xa.rh_l[2], xa.rh_l[3], xa.phase);
            buf[written * 2 + 1] = hermite4(
                xa.rh_r[0], xa.rh_r[1], xa.rh_r[2], xa.rh_r[3], xa.phase);
        }
        xa.phase += XA_STEP_Q16;
        written++;
    }
end:
    if (hit_eof) {
        is_playing = 0;
        xa.active = 0;
    }
    Psyz_AudioUnlock();
    if (hit_eof && CD_cbready) {
        CD_cbready(CdlDataEnd, NULL);
    }
    return written;
}

size_t Psyz_CdPullSamples(short* out, size_t num_frames) {
    size_t read_frames;
    if (CD_mode & CdlModeDA) {
        read_frames = cdda_pull_samples(out, num_frames);
    } else if (CD_mode & CdlModeRT) {
        read_frames = xa_pull_samples(out, num_frames);
    } else {
        read_frames = 0;
    }
    if (!enable_passthrough) {
        for (size_t i = 0; i < read_frames; i++) {
            int l = out[i * 2 + 0];
            int r = out[i * 2 + 1];
            int l_out = (l * ctx.cd_src_l_dst_l + r * ctx.cd_src_r_dst_l) >> 7;
            int r_out = (l * ctx.cd_src_l_dst_r + r * ctx.cd_src_r_dst_r) >> 7;
            out[i * 2 + 0] = clamp16(l_out);
            out[i * 2 + 1] = clamp16(r_out);
        }
    }
    return read_frames;
}

// Open the track containing the absolute sector encoded in CD_pos. Closes
// any previously-open track_file. Returns 0 on success, -1 on failure.
static int open_track_at_cd_pos(void) {
    int sector = CdPosToInt(&CD_pos);
    TrackEntry* track = NULL;
    for (int i = 0; i < g_track_count; i++) {
        if (!g_tracks[i].is_valid) {
            continue;
        }
        int next_abs_sector = INT_MAX;
        if (i + 1 < g_track_count && g_tracks[i + 1].is_valid) {
            next_abs_sector = g_tracks[i + 1].abs_sector;
        }
        if (sector >= g_tracks[i].abs_sector && sector < next_abs_sector) {
            track = &g_tracks[i];
            break;
        }
    }
    if (!track) {
        WARNF("no track found for sector %d", sector);
        return -1;
    }
    if (track_file) {
        fclose(track_file);
        track_file = NULL;
    }
    FILE* file = fopen(track->file_path, "rb");
    if (!file) {
        ERRORF("failed to open audio file: %s", track->file_path);
        return -1;
    }
    int sector_offset = sector - track->abs_sector;
    sector_offset += track->start_sector;
    long byte_offset = (long)sector_offset * SECTOR_SIZE;
    if (fseek(file, byte_offset, SEEK_SET) != 0) {
        ERRORF("failed to seek to sector %d in %s", sector, track->file_path);
        fclose(file);
        return -1;
    }
    DEBUGF("opened track %s at sector %d (offset %d)", track->file_path, sector,
           sector_offset);
    track_file = file;
    return 0;
}

static int need_cdda_rewind = 1;
// play on CDDA mode only (not XA)
static void psyz_play() {
    if (!(CD_mode & CdlModeDA)) {
        WARNF("CdlModeDA not active, audio will not be played");
        return;
    }
    if (is_playing) {
        DEBUGF("audio already playing");
        return;
    }
    if (!need_cdda_rewind) {
        Psyz_AudioLock();
        is_playing = 1;
        Psyz_AudioUnlock();
        return;
    }
    if (open_track_at_cd_pos() != 0) {
        return;
    }
    Psyz_AudioLock();
    cd_buf_pos = 0;
    cd_buf_count = 0;
    need_cdda_rewind = 0;
    is_playing = 1;
    Psyz_AudioUnlock();
}

// Begin XA streaming at CD_pos (CdlReadN / CdlReadS).
static void psyz_xa_read(void) {
    if (!(CD_mode & CdlModeRT)) {
        LOG_ONCE("CdlReadN/ReadS without CdlModeRT not implemented");
        return;
    }
    if (open_track_at_cd_pos()) {
        return;
    }
    Psyz_AudioLock();
    xa_reset_stream();
    xa.cur_abs_sector = CdPosToInt(&CD_pos) - 1;
    xa.active = 1;
    is_playing = 1;
    Psyz_AudioUnlock();
}

static void psyz_stop() {
    need_cdda_rewind = 1;
    Psyz_AudioLock();
    is_playing = 0;
    xa.active = 0;
    if (track_file) {
        fclose(track_file);
        track_file = NULL;
    }
    cd_buf_pos = 0;
    cd_buf_count = 0;
    Psyz_AudioUnlock();
}

static void psyz_pause() {
    Psyz_AudioLock();
    is_playing = 0;
    xa.active = 0;
    Psyz_AudioUnlock();
}

static void psyz_mute() {
    Psyz_AudioLock();
    is_muted = 1;
    Psyz_AudioUnlock();
}

static void psyz_demute() {
    Psyz_AudioLock();
    is_muted = 0;
    Psyz_AudioUnlock();
}

static void callback(void) { NOT_IMPLEMENTED; }

int CD_init(void) {
    u_char nop_result[8];
    CD_com = 0;
    CD_mode = 0;
    CD_cbready = 0;
    CD_cbsync = 0;
    CD_status1 = 0;
    CD_status = 0;
    ResetCallback();
    InterruptCallback(2, &callback);
    CD_cw(CdlNop, 0, nop_result, 0);
    if (CD_status & CdlStatShellOpen) {
        CD_cw(CdlNop, 0, nop_result, 0);
    }
    if (CD_cw(CdlReset, 0, 0, 0)) {
        return -1;
    }
    if (CD_cw(CdlDemute, 0, 0, 0)) {
        return -1;
    }
    if (CD_sync(0, 0) != 2) {
        return -1;
    }
    return 0;
}

void CD_initintr(void) { NOT_IMPLEMENTED; }
void CD_flush(void) { NOT_IMPLEMENTED; }

int CD_vol(CdlATV* vol) {
    // 0x1F801800 = 2
    ctx.cd_src_l_dst_l = vol->val0; // 0x1F801802
    ctx.cd_src_l_dst_r = vol->val1; // 0x1F801803

    // 0x1F801800 = 3
    ctx.cd_src_r_dst_r = vol->val2; // 0x1F801801
    ctx.cd_src_r_dst_l = vol->val3; // 0x1F801802
    CHNGATV();                      // 0x1F801803 = 0x20
    return 0;
}

int CD_getsector(void* madr, int size) { NOT_IMPLEMENTED; }
int CD_getsector2(void) { NOT_IMPLEMENTED; }
void CD_datasync(int mode) { NOT_IMPLEMENTED; }

int CD_initvol(void) {
    CdlATV vol = {128, 0, 128, 0};
    if (SPUR(main_volx.left) == 0 && SPUR(main_volx.right) == 0) {
        SPUW(main_vol.left, 0x3FFF);
        SPUW(main_vol.right, 0x3FFF);
    }
    SPUW(cd_vol.left, 0x3FFF);
    SPUW(cd_vol.right, 0x3FFF);
    SPUW(spucnt, SPU_CTRL_MASK_SPU_ENABLE | SPU_CTRL_MASK_MUTE_SPU |
                     SPU_CTRL_MASK_CD_AUDIO_ENABLE);
    CD_vol(&vol);
    return 0;
}

int CD_sync(int mode, u_char* result) {
    if (result) {
        memcpy(result, last_result, sizeof(last_result));
    }
    return last_intr;
}
int CD_ready(int mode, u_char* result) {
    NOT_IMPLEMENTED;
    return CdlComplete;
}
int CD_cw(u_char com, u_char* param, u_char* result, s32 arg3) {
    int total;
    int t;
    int abs_sector;

    CD_sync(0, 0);
    switch (com) {
    case CdlNop:
        if (shell_is_open) {
            if (result) {
                result[0] = (u_char)(CD_status | CdlStatError);
                result[1] = 0x80;
                result[2] = 0x00;
                result[3] = 0x00;
                cache_last_result(result, CdlDiskError);
            }
            return -1;
        }
        if (result) {
            result[0] = (u_char)CD_status;
            result[1] = 0x00;
            result[2] = 0x00;
            result[3] = 0x00;
            cache_last_result(result, CdlComplete);
        }
        break;
    case CdlSetloc:
        if (!param) {
            ERRORF("%s got NULL param", CD_comstr[com]);
            return -2;
        }
        CD_pos = *(CdlLOC*)param;
        break;
    case CdlPlay:
        psyz_play();
        break;
    case CdlReadN:
    case CdlReadS:
        psyz_xa_read();
        break;
    case CdlSetfilter:
        if (!param) {
            ERRORF("%s got NULL param", CD_comstr[com]);
            return -2;
        }
        xa.filter_file = param[0];
        xa.filter_channel = param[1];
        xa.filter_set = 1;
        break;
    case CdlStop:
        psyz_stop();
        break;
    case CdlPause:
        psyz_pause();
        break;
    case CdlMute:
        psyz_mute();
        break;
    case CdlDemute:
        psyz_demute();
        break;
    case CdlSetmode:
        if (!param) {
            ERRORF("%s got NULL param", CD_comstr[com]);
            return -2;
        }
        if (*param & CdlModeDA) {
            Psyz_AudioInit();
        }
        if (*param & CdlModeRT) {
            Psyz_AudioInit();
        }
        if (*param & CdlModeAP) {
            LOG_ONCE("%s does not support CdlModeAP", CD_comstr[com]);
        }
        if (*param & CdlModeRept) {
            LOG_ONCE("%s does not support CdlModeRept", CD_comstr[com]);
        }
        if (*param & CdlModeSize0) {
            LOG_ONCE("%s does not support CdlModeSize0", CD_comstr[com]);
        }
        if (*param & CdlModeSize1) {
            LOG_ONCE("%s does not support CdlModeSize1", CD_comstr[com]);
        }
        if (*param & CdlModeSpeed) {
            LOG_ONCE("%s does not support CdlModeSpeed", CD_comstr[com]);
        }
        if (*param & CdlModeStream2) {
            LOG_ONCE("%s does not support CdlModeStream2", CD_comstr[com]);
        }
        if (*param & CdlModeStream) {
            LOG_ONCE("%s does not support CdlModeStream", CD_comstr[com]);
        }
        CD_mode = *param;
        break;
    case CdlGetlocL: {
        if (!result) {
            ERRORF("%s got NULL result", CD_comstr[com]);
            return -2;
        }
        // always return XA sector if XA playback is active
        int cur = xa.active ? xa.cur_abs_sector : CdPosToInt(&CD_pos);
        if (cur < 0) {
            cur = 0;
        }
        CdlLOC loc;
        CdIntToPos(cur, &loc);
        result[0] = loc.minute;
        result[1] = loc.second;
        result[2] = loc.sector;
        result[3] = 0x01;
        result[4] = xa.filter_file;
        result[5] = xa.filter_channel;
        result[6] = 0x00;
        result[7] = 0x00;
        u_char status_result[8] = {(u_char)CD_status, 0, 0, 0, 0, 0, 0, 0};
        cache_last_result(status_result, CdlComplete);
        break;
    }
    case CdlGetlocP:
        LOG_ONCE("com %s not implemented", CD_comstr[com]);
        break;
    case CdlGetTN:
        if (!result) {
            ERRORF("%s got NULL result", CD_comstr[com]);
            return -2;
        }
        if (shell_is_open || !is_disk_loaded || g_track_count == 0) {
            result[0] = (u_char)(CD_status | CdlStatError);
            result[1] = 0x80; // error-response sentinel
            result[2] = 0x00;
            result[3] = 0x00;
            cache_last_result(result, CdlDiskError);
            return -1;
        }
        result[0] = (u_char)CD_status;
        result[1] = itob(1);
        result[2] = itob(g_track_count);
        result[3] = 0x00;
        cache_last_result(result, CdlComplete);
        break;
    case CdlGetTD:
        if (!param || !result) {
            ERRORF("%s got NULL param/result", CD_comstr[com]);
            return -2;
        }
        if (shell_is_open || !is_disk_loaded || g_track_count == 0) {
            result[0] = (u_char)(CD_status | CdlStatError);
            result[1] = 0x80;
            result[2] = 0x00;
            result[3] = 0x00;
            cache_last_result(result, CdlDiskError);
            return -1;
        }
        t = btoi(param[0]);
        abs_sector;
        if (t == 0) {
            abs_sector = lead_out_sector();
        } else if (t < 1 || t > g_track_count || !g_tracks[t - 1].is_valid) {
            result[0] = (u_char)(CD_status | CdlStatError);
            result[1] = 0x80;
            result[2] = 0x00;
            result[3] = 0x00;
            cache_last_result(result, CdlDiskError);
            return -1;
        } else {
            abs_sector = g_tracks[t - 1].abs_sector;
        }
        // The +150 lead-in pregap is part of the controller-reported MSF.
        total = abs_sector + 150;
        result[0] = (u_char)CD_status;
        result[1] = itob(total / 75 / 60);
        result[2] = itob(total / 75 % 60);
        result[3] = 0x00;
        cache_last_result(result, CdlComplete);
        break;
    default:
        if (com >= LEN(CD_comstr)) {
            ERRORF("com %X invalid", com);
            return -1;
        }
        LOG_ONCE("com %s not implemented", CD_comstr[com]);
    }
    return 0;
}

int CdInit(void) {
    NOT_IMPLEMENTED;
    CdReset(1);
    return CD_init();
}

int CdReading() {
    NOT_IMPLEMENTED;
    return 0;
}

void ExecCd() { NOT_IMPLEMENTED; }

int CdRead(int sectors, u_long* buf, int mode) {
    NOT_IMPLEMENTED;
    return 0;
}

int CdRead2(long mode) {
    NOT_IMPLEMENTED;
    return 0;
}

int CdReadSync(int mode, u_char* result) {
    NOT_IMPLEMENTED;
    return 0;
}

u_long StGetNext(u_long** addr, u_long** header) {
    NOT_IMPLEMENTED;
    return 0;
}

void StSetRing(u_long* ring_addr, u_long ring_size) { NOT_IMPLEMENTED; }

void StSetStream(u_long mode, u_long start_frame, u_long end_frame,
                 void (*func1)(), void (*func2)()) {
    NOT_IMPLEMENTED;
}

u_long StFreeRing(u_long* base) {
    NOT_IMPLEMENTED;
    return 0;
}
