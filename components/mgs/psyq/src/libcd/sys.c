#include <common.h>
#include <libcd.h>

#ifndef CdlSync
#define CdlSync 0
#endif

extern CdlCB CD_cbsync;
extern CdlCB CD_cbready;
extern u_char CD_status;
extern u_char CD_mode;
extern u_char CD_com;
extern CdlLOC CD_pos;
extern char* CD_comstr[];
extern char* CD_intstr[];

int CD_init(void);
void CD_initintr(void);
void CD_flush(void);
int CD_initvol(void);
int CD_sync(int mode, u_char* result);
int CD_ready(int mode, u_char* result);
int CD_cw(u8 com, u8* param, u_char* result, s32 arg3);
int CD_vol(CdlATV* vol);
int CD_getsector(void* madr, int size);
int CD_getsector2(void* madr, int size);
int CD_datasync(int mode);
CdlCB DMACallback(int mode, CdlCB func);

static int D_800B5718[] = {
    0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0,
};

int CdStatus(void) { return CD_status; }

int CdMode(void) { return CD_mode; }

int CdLastCom(void) { return CD_com; }

CdlLOC* CdLastPos(void) { return &CD_pos; }

int CdReset(int mode) {
    if (mode == CdlModeAP) {
        CD_initintr();
        return 1;
    }

    if (CD_init()) {
        return 0;
    }

    if (mode == CdlModeDA && CD_initvol()) {
        return 0;
    }

    return 1;
}

void CdFlush(void) { CD_flush(); }

extern int CD_debug;
int CdSetDebug(int level) {
    int prev = CD_debug;
    CD_debug = level;
    return prev;
}

char* CdComstr(unsigned char com) {
    if (com > 27) {
        return "none";
    }

    return CD_comstr[com];
}

char* CdIntstr(u8 intr) {
    if (intr > 6) {
        return "none";
    }
    return CD_intstr[intr];
}

int CdSync(int mode, u_char* result) { return CD_sync(mode, result); }

int CdReady(int mode, u_char* result) { return CD_ready(mode, result); }

CdlCB CdSyncCallback(CdlCB func) {
    CdlCB prev = CD_cbsync;
    CD_cbsync = func;
    return prev;
}

CdlCB CdReadyCallback(CdlCB func) {
    CdlCB prev = CD_cbready;
    CD_cbready = func;
    return prev;
}

static inline int cd_cw(u8 com, u8* param, u_char* result, s32 arg3) {
    CdlCB old = CD_cbsync;
    int count = 4;

    while (count--) {
        CD_cbsync = NULL;
        if (com != CdlNop && (CD_status & CdlStatShellOpen)) {
            CD_cw(CdlNop, NULL, NULL, 0);
        }
        if (param == NULL || D_800B5718[com] == 0 ||
            !CD_cw(CdlSetloc, param, result, 0)) {
            CD_cbsync = old;
            if (!CD_cw(com, param, result, arg3)) {
                return 0;
            }
        }
    }

    CD_cbsync = old;
    return -1;
}

int CdControl(u_char com, u_char* param, u_char* result) {
    return cd_cw(com, param, result, 0) == 0;
}

int CdControlF(u_char com, u_char* param) {
    return cd_cw(com, param, NULL, 1) == 0;
}

int CdControlB(u_char com, u_char* param, u_char* result) {
    if (cd_cw(com, param, result, 0)) {
        return 0;
    }
    return CD_sync(CdlSync, result) == CdlComplete;
}

int CdMix(CdlATV* vol) {
    CD_vol(vol);
    return 1;
}

int CdGetSector(void* madr, int size) { return CD_getsector(madr, size) == 0; }

int CdGetSector2(void* madr, int size) {
    return CD_getsector2(madr, size) == 0;
}

CdlCB CdDataCallback(CdlCB func) { return DMACallback(3, func); }

int CdDataSync(int mode) { return CD_datasync(mode); }

static inline int ENCODE_BCD(int n) { return ((n / 10) << 4) + (n % 10); }
CdlLOC* CdIntToPos(int i, CdlLOC* p) {
    i += 150;
    p->sector = ENCODE_BCD(i % 75);
    p->second = ENCODE_BCD(i / 75 % 60);
    p->minute = ENCODE_BCD(i / 75 / 60);
    return p;
}

int CdPosToInt(CdlLOC* p) {
#define DECODE_BCD(x) (((x) >> 4) * 10 + ((x) & 0xF))
    u8 sector = p->sector;
    u8 second = p->second;
    u8 minute = p->minute;

    return (DECODE_BCD(minute) * 60 + DECODE_BCD(second)) * 75 +
           DECODE_BCD(sector) - 150;
}
