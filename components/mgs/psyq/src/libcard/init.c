#include <common.h>
#include <kernel.h>
#include <libapi.h>

static u8 D_800B50E0[] = {0x50, 0x73, 0x03, 0x15, 0x9C, 0x85, 0x40};

void InitCARD2(long val);
int ReadInitPadFlag(void);
void InitCARD(long val) {
    ChangeClearPAD(0);
    EnterCriticalSection();
    if (!ReadInitPadFlag()) {
        val = 0;
    }
    InitCARD2(val);
    ExitCriticalSection();
}

void StartCARD2(void);
long StartCARD(void) {
    EnterCriticalSection();
    StartCARD2();
    ChangeClearPAD(0);
    ExitCriticalSection();
}

void StopCARD2(void);
void _ExitCard(void);
long StopCARD(void) {
    StopCARD2();
    _ExitCard();
}
