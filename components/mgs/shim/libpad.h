/* PSY-Q libpad.h stand-in: psyz declares the pad functions in libetc.h but not
 * the state / actuator-info constants that mts/mts_pad.c switches on. */
#ifndef MGS_SHIM_LIBPAD_H
#define MGS_SHIM_LIBPAD_H

#include <libetc.h>

/* PadGetState() */
#define PadStateDiscon   0
#define PadStateFindPad  1
#define PadStateFindCTP1 2
#define PadStateReqInfo  5
#define PadStateExecCmd  6
#define PadStateStable   7

/* PadInfoMode() */
#define InfoModeCurID     1
#define InfoModeCurExID   2
#define InfoModeCurExOffs 3
#define InfoModeIdTable   4

/* PadInfoAct() */
#define InfoActFunc 1
#define InfoActSub  2
#define InfoActSize 3
#define InfoActCurr 4
#define InfoActSign 5

#endif
