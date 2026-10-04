#include <psyz.h>
#include <libetc.h>

#include "libapi.h"

#include <psyz/log.h>

void MyPadInit(int mode);
void PadInit(int mode) { MyPadInit(mode); }

static unsigned UnpackPadBits(const char* buf) {
    if ((unsigned char)buf[0] != 0) {
        return 0; // port disconnected or frame not valid
    }
    switch ((unsigned char)buf[1]) {
    case PSYZ_CTRL_DIGITAL_PAD:
    case PSYZ_CTRL_ANALOG_STICK:
    case PSYZ_CTRL_ANALOG_PAD:
        break;
    default:
        return 0; // not a pad-like device, switches are not in bytes 2-3
    }
    unsigned int buttons0 = (unsigned char)buf[2];
    unsigned int buttons1 = (unsigned char)buf[3];
    unsigned int padq = (buttons0 << 8) | buttons1;
    return (~padq) & 0xFFFF;
}

unsigned int PadRead(int id) {
    char p0[PSYZ_PAD_BUF_LEN], p1[PSYZ_PAD_BUF_LEN];
    Psyz_PadsGet(0, p0, sizeof(p0));
    Psyz_PadsGet(1, p1, sizeof(p1));
    unsigned r = UnpackPadBits(p0) | (UnpackPadBits(p1) << 16);

    // PS1 gamepad cannot have opposite D-pad pressed at the same time
    // solves a bug on Castlevania SOTN when pressing both Left and Right
    // where its behavior is undefined.
    if (r & PADLleft && r & PADLright) {
        r &= ~(PADLleft | PADLright);
    }
    if (r & PADLup && r & PADLdown) {
        r &= ~(PADLup | PADLdown);
    }

    return r;
}

void PadStop(void) { NOT_IMPLEMENTED; }

void (*g_VsyncCallback)() = NULL;

int VSyncCallback(void (*f)()) {
    /* The PSX raises a vblank interrupt 60 times a second and the BIOS calls
     * this hook from it. Games build their whole frame pacing on it -- MGS's
     * mts scheduler blocks in mts_wait_vbl() until this fires. Store it here;
     * the platform layer is responsible for calling g_VsyncCallback at the
     * field rate. */
    g_VsyncCallback = f;
    return 0;
}

int VSyncCallbacks(int ch, void (*f)()) { NOT_IMPLEMENTED; }

static long video_mode = 0;
long GetVideoMode() { return video_mode; }
long SetVideoMode(long mode) {
    long prev = video_mode;
    video_mode = mode;
    return prev;
}

int SetIntrMask(int arg) {
    NOT_IMPLEMENTED;
    return 0;
}

int StopCallback(void) { NOT_IMPLEMENTED; }
int ResetCallback(void) { NOT_IMPLEMENTED; }
void* DMACallback(int dma, void (*func)()) {
    NOT_IMPLEMENTED;
    return NULL;
}

#define RCNT2_CLOCK_HZ 4233600 // 1/8 of the ~33.8688 MHz PS1 CPU clock

static struct {
    unsigned rate_hz;      // tick rate this counter is programmed to, 0=stopped
    unsigned accum;        // timer accumulating towards 44100 Hz, used to tick
    void (*handler)(void); // interrupt handler bound to this counter, or NULL
} rcnt2, rcnt3;

static unsigned set_counter = 0;

void* InterruptCallback(int arg0, void (*cb)()) {
    if (arg0 < 0 || arg0 >= 0x100) {
        return NULL;
    }
    void (*prev)(void) = NULL;
    switch (set_counter) {
    case 2: // RCntCNT2
        prev = rcnt2.handler;
        if (cb) {
            rcnt2.handler = cb;
            set_counter = 0;
        }
        break;
    case 3: // RCntCNT3
        prev = rcnt3.handler;
        if (cb) {
            rcnt3.handler = cb;
            set_counter = 0;
        }
        break;
    }
    return prev;
}

int InitTAP(char* bufA, long lenA, char* bufB, long lenB) {
    LOG_ONCE("forwarding to InitPAD");
    return InitPAD(bufA, lenA, bufB, lenB);
}

void StartTAP(void) {
    LOG_ONCE("forwarding to StartPAD");
    StartPAD();
}

__attribute__((weak)) int PadGetState(int port) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
    return 0;
}

__attribute__((weak)) void PadSetAct(int port, u_char* data, int len) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
}

__attribute__((weak)) int PadSetActAlign(int port, char* data) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
    return 0;
}

void PadInitMtap(u_char* pad1, u_char* pad2) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
}

__attribute__((weak)) void PadStartCom(void) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
}

static unsigned rcnt_counter(long spec) {
    return (unsigned)(spec & 0xFF); // strip Desc 0xFX000000
}

long SetRCnt(long spec, unsigned short target, long mode) {
    set_counter = rcnt_counter(spec);
    switch (set_counter) {
    case 2: // RCntCNT2
        rcnt2.rate_hz = target ? (unsigned)(RCNT2_CLOCK_HZ / target) : 0;
        rcnt2.accum = 0;
        break;
    case 3: // RCntCNT3
        rcnt3.rate_hz = (GetVideoMode() == MODE_PAL) ? 50 : 60;
        rcnt3.accum = 0;
        break;
    }
    return 1;
}

long ResetRCnt(long spec) {
    switch (rcnt_counter(spec)) {
    case 2:
        rcnt2.accum = 0;
        break;
    case 3:
        rcnt3.accum = 0;
        break;
    }
    return 1;
}

static void rcnt_add(int* accum_state, int rate_hz, int n, void (*cb)(void)) {
    if (rate_hz <= 0 || !cb) {
        return;
    }
    int accum = *accum_state + rate_hz * n;
    while (accum >= PSYZ_SPU_SAMPLE_RATE) {
        accum -= PSYZ_SPU_SAMPLE_RATE;
        cb();
    }
    *accum_state = accum;
}

void Psyz_RcntAdd(int n) {
    if (n <= 0) {
        return;
    }
    rcnt_add(&rcnt2.accum, rcnt2.rate_hz, n, rcnt2.handler);
    rcnt_add(&rcnt3.accum, rcnt3.rate_hz, n, rcnt3.handler);
}
