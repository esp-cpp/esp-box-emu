/* Gamepad input for Metal Gear Solid on esp-box-emu.
 *
 * mts_pad.c registers two PAD_RECV_BUF buffers through PadInitDirect() and
 * parses them every vsync as raw SIO pad frames: byte 0 result (0 = ok), byte 1
 * terminal type / size (0x41 = digital pad, one halfword), bytes 2-3 the button
 * state, ACTIVE LOW. PadGetState() drives its discovery state machine.
 *
 * Bit order (button_hi << 8 | button_lo, libgv.h's PAD_* constants):
 *   hi: 0x80 LEFT  0x40 DOWN  0x20 RIGHT  0x10 UP
 *       0x08 START 0x04 R3    0x02 L3     0x01 SELECT
 *   lo: 0x80 SQUARE 0x40 CROSS 0x20 CIRCLE 0x10 TRIANGLE
 *       0x08 R1     0x04 L1    0x02 R2     0x01 L2
 *
 * The emulator loop (src/mgs.cpp) maps the box gamepad onto these bits into
 * mgs_box_pad; the vblank tick calls Mgs_PadsUpdate() and writes the frames.
 */
#include <stdio.h>
#include "mgs_glue.h"

#define PadStateFindCTP1 2

volatile uint16_t mgs_box_pad;

static unsigned char* sPadBuf[2];

void PadInitDirect(unsigned char* pad1, unsigned char* pad2) {
    sPadBuf[0] = pad1;
    sPadBuf[1] = pad2;
    printf("[pad] buffers registered\n");
}

int PadGetState(int port) {
    /* "a plain controller is present" on both ports, every poll */
    (void)port;
    return PadStateFindCTP1;
}

void PadSetAct(int port, unsigned char* data, int len) {
    (void)port; (void)data; (void)len; /* no rumble hardware */
}

int PadSetActAlign(int port, char* data) {
    (void)port; (void)data;
    return 0;
}

void PadStartCom(void) {}

/* called by the vblank tick (esp32_vblank.c) right before the mts callback */
void Mgs_PadsUpdate(void) {
    const unsigned pressed = mgs_box_pad;
    if (!sPadBuf[0]) {
        return;
    }
    /* port 0: digital pad frame, active low */
    sPadBuf[0][0] = 0x00;
    sPadBuf[0][1] = 0x41;
    sPadBuf[0][2] = (unsigned char)(~(pressed >> 8) & 0xFF);
    sPadBuf[0][3] = (unsigned char)(~pressed & 0xFF);
    /* port 1: nothing connected */
    if (sPadBuf[1]) {
        sPadBuf[1][0] = 0xFF;
    }
}

/* called once per frame from the game's own loop (libdg/dgd.c); the box pad is
 * a real held state, nothing to age */
void Mgs_PadConsumed(void) {}
