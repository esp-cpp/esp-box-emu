#include <psyz.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define PSYZ_ESP_XRAM EXT_RAM_BSS_ATTR
#else
#define PSYZ_ESP_XRAM
#endif
#include <psyz/log.h>
#include <kernel.h>
#include <libetc.h>
#include "libgpu.h"
#include "../draw.h"
#include "../internal.h"

#undef _get_errno // Windows: avoid conflicts
#undef undelete   // macOS: avoid conflicts

typedef struct {
    char frame[PSYZ_PAD_BUF_LEN];   // last frame published by the platform
    PsyzControllerKind channels[4]; // only channel 0 until multitap lands
} ControllerPort;

static ControllerPort ports[2] = {
    {{0},
     {
         PSYZ_CTRL_DIGITAL_PAD,
         PSYZ_CTRL_DISCONNECTED,
         PSYZ_CTRL_DISCONNECTED,
         PSYZ_CTRL_DISCONNECTED,
     }},
    {{0},
     {
         PSYZ_CTRL_DIGITAL_PAD,
         PSYZ_CTRL_DISCONNECTED,
         PSYZ_CTRL_DISCONNECTED,
         PSYZ_CTRL_DISCONNECTED,
     }},
};

static int pads_sampled = 0; // avoid more than one input polling per frame
static char* pad_buffers[2];
static int pad_buffer_lens[2];
static void ReadPadsOnVsync(void) {
    pads_sampled = 0;
    for (int p = 0; p < LEN(ports); p++) {
        if (pad_buffers[p]) {
            Psyz_PadsGet(p, pad_buffers[p], pad_buffer_lens[p]);
        }
    }
}

static PsyzVSyncCb g_PsyzVsyncCb = NULL;
void Psyz_SetVSyncCb(PsyzVSyncCb cb) { g_PsyzVsyncCb = cb; }

extern void (*g_VsyncCallback)();
int Psyz_VideoVSync(int mode);
int VSync(int mode) {
    int raw = Psyz_VideoVSync(mode);
    if (mode < 0) {
        /* Total vertical blanks since boot. The PSX returns Vcount, a full
         * 32-bit counter -- roughly 800 days before it wraps.
         *
         * This used to truncate to 16 bits, which put a hard 65536-field limit
         * on the machine: MGS's scheduler stores mts_time = VSync(-1) and
         * parks a task with target = last + count, so at 17.6 minutes the
         * clock rolled back to zero while a task sat waiting for target 65537.
         * "mts_time < target" then stayed true forever and the game froze with
         * every counter repeating and one task never waking. Truncation is
         * right for mode 1 -- that one really is a 16-bit hblank delta the
         * hardware reports masked -- but not for the field count. */
        return raw;
    }
    if (mode == 1) {
        return (unsigned short)raw;
    }
    ReadPadsOnVsync(); // this is done on vsync by the BIOS
    if (g_PsyzVsyncCb) {
        g_PsyzVsyncCb();
    }
    if (g_VsyncCallback) {
        g_VsyncCallback();
    }
    return raw;
}

PsyzControllerKind Psyz_PadsSetKind(
    int port, int channel, PsyzControllerKind kind) {
    if (port < 0 || port >= LEN(ports)) {
        return PSYZ_CTRL_ERROR;
    }
    if (channel < 0 || channel >= LEN(ports->channels)) {
        return PSYZ_CTRL_ERROR;
    }
    if (kind == PSYZ_CTRL_QUERY_KIND) {
        return ports[port].channels[channel];
    }
    PsyzControllerKind prev = ports[port].channels[channel];
    ports[port].channels[channel] = kind;
    return prev;
}

void Psyz_PadsSet(int port, const char* src, int len) {
    if (port < 0 || port >= LEN(ports) || !src || len <= 0) {
        return;
    }
    if (len > PSYZ_PAD_BUF_LEN) {
        len = PSYZ_PAD_BUF_LEN;
    }
    memcpy(ports[port].frame, src, len);
    pads_sampled = 1;
}

void Psyz_PadsPoll(void); // implemented by the platform layer
void Psyz_PadsGet(int port, char* dst, int len) {
    if (port < 0 || port >= LEN(ports) || !dst || len <= 0) {
        return;
    }
    if (!pads_sampled) {
        pads_sampled = 1;
        Psyz_PadsPoll();
    }
    if (len > PSYZ_PAD_BUF_LEN) {
        len = PSYZ_PAD_BUF_LEN;
    }
    memcpy(dst, ports[port].frame, len);
}

int InitPAD(char* bufA, int lenA, char* bufB, int lenB) {
    PadInit(0);
    if (bufA) {
        memset(bufA, 0, lenA);
    } else {
        WARNF("bufA is NULL");
    }
    if (bufB) {
        memset(bufB, 0, lenB);
    } else {
        WARNF("bufB is NULL");
    }
    pad_buffers[0] = bufA;
    pad_buffer_lens[0] = lenA;
    pad_buffers[1] = bufB;
    pad_buffer_lens[1] = lenB;
    return 1;
}

long StartPAD(void) {
    NOT_IMPLEMENTED;
    return 1;
}

void StopPAD(void) { NOT_IMPLEMENTED; }

int PAD_init(int type, void* unused) {
    PadInit(0);
    return 1;
}

int PAD_dr(int port, char* dst) {
    if (port < 0 || port > 1 || !dst) {
        return 0;
    }
    Psyz_PadsGet(port, dst, PSYZ_PAD_BUF_LEN);
    return PSYZ_PAD_BUF_LEN;
}

void _96_remove(void) { NOT_IMPLEMENTED; }

long ReadInitPadFlag(void) {
    NOT_IMPLEMENTED;
    return 0;
}

void ChangeClearPAD(long a) { NOT_IMPLEMENTED; }

static unsigned long event_first_empty = 0;
PSYZ_ESP_XRAM static struct EvCB events[0x100];
static long GetFirstFreeEvent() {
    // event_first_empty brings the function to O(1) in an optimistic scenario,
    // but it does not guarantee it always points to an empty event
    // Search from event_first_empty to end
    for (unsigned long i = event_first_empty; i < LEN(events); i++) {
        if (!events[i].desc) {
            return (long)i;
        }
    }
    // Search from beginning to event_first_empty (wrap around)
    for (unsigned long i = 0; i < event_first_empty; i++) {
        if (!events[i].desc) {
            return (long)i;
        }
    }
    return -1;
}
long OpenEvent(unsigned long desc, long spec, long mode, long (*func)()) {
    if (!desc) {
        WARNF("invalid desc %08X", desc);
        return -1;
    }
    long id = GetFirstFreeEvent();
    if (id < 0) {
        WARNF("run out of memory");
        return -1;
    }
    struct EvCB* e = &events[id];
    e->desc = desc;
    e->spec = (int)spec;
    e->mode = (int)mode;
    e->FHandler = func;
    e->system[0] = 0;
    e->system[1] = 0;
    int supported = 1;
    switch (desc) {
    case SwCARD:
        switch (spec) {
        case EvSpIOE: // always report memory card as connected
            e->status = 1;
            break;
        case EvSpERROR:  // never errors
        case EvSpTIMOUT: // never report memory card as disconnected
        case EvSpNEW:    // never block writing after connection
            e->status = 0;
            break;
        default:
            supported = 0;
            break;
        }
        break;
    case HwCARD:
        switch (spec) {
        case EvSpIOE: // always report end of IO
            e->status = 1;
            break;
        case EvSpERROR:  // never errors
        case EvSpTIMOUT: // never timeout
        case EvSpNEW:    // never report a new memory card
            e->status = 0;
            break;
        default:
            supported = 0;
            break;
        }
        break;
    default:
        supported = 0;
        break;
    }
    if (!supported) {
        e->status = 0;
        WARNF("unsupported spec:%08X, desc:%04X, mode:%04X", spec, desc, mode);
    }
    event_first_empty = id + 1;
    // Wrap around if event_first_empty is beyond array bounds
    if (event_first_empty >= LEN(events)) {
        event_first_empty = 0;
    }
    return id;
}
long CloseEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    events[event].desc = 0;
    event_first_empty = event;
    return 1;
}
long WaitEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    // never waits
    return 1;
}
long EnableEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    NOT_IMPLEMENTED;
    return 1;
}
long DisableEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    NOT_IMPLEMENTED;
    return 1;
}
long TestEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    return events[event].status;
}

/* The PSX masked interrupts here, and cooperative schedulers built on the
 * kernel (MGS's mts) guard ALL their bookkeeping with this. Hosts that model
 * the vblank as a preempting task must check this depth and defer their tick
 * while it is nonzero, or they preempt half-written scheduler state. */
volatile int psyz_critical_depth;

/* NOT a counter: the PSX syscall set the interrupt mask absolutely, and code
 * exploits that -- mts tasks begin with a bare ExitCriticalSection to enable
 * interrupts they never disabled. Counting drifts upward and masks the tick
 * forever. */
void PS1_EnterCriticalSection(void) { psyz_critical_depth = 1; }
void PS1_ExitCriticalSection(void) { psyz_critical_depth = 0; }

void DeliverEvent(unsigned ev1, unsigned ev2) { NOT_IMPLEMENTED; }

void UnDeliverEvent(unsigned ev1, unsigned ev2) { NOT_IMPLEMENTED; }

void SystemError(char c, long n) {
    NOT_IMPLEMENTED;
    ERRORF("SystemError('%c', 0x%X)", c, n);
}

struct DIRENTRY* my_firstfile(char* dirPath, struct DIRENTRY* firstEntry);
struct DIRENTRY* firstfile(char* dirPath, struct DIRENTRY* firstEntry) {
    return my_firstfile(dirPath, firstEntry);
}

struct DIRENTRY* my_nextfile(struct DIRENTRY* outEntry);
struct DIRENTRY* nextfile(struct DIRENTRY* outEntry) {
    return my_nextfile(outEntry);
}

long my_erase(char* path);
long erase(char* path) { return my_erase(path); }

long psyz_undelete(char* name) {
    NOT_IMPLEMENTED;
    return 0;
}

long my_format(char* fs);
long format(char* fs) { return my_format(fs); }

int psyz_get_errno(void) {
    NOT_IMPLEMENTED;
    return 0;
}
