"""Play MGS on the ESP32-S3 with the PC keyboard over the serial port.

    python port\\play.py [COM6]

Keys:

    W/A/S/D   D-pad (up/left/down/right)
    E         CIRCLE  - action: doors, talk, confirm in menus
    X         CROSS   - crouch
    Z         SQUARE  - punch
    Q         TRIANGLE - first person
    1/2/3/4   L1/R1/L2/R2
    ENTER     START
    SPACE     SELECT
    ESC       quit this script (the game keeps running on the board)

Buttons are HELD for as long as the key is down. That needs the real key state,
not console characters: a console tells you a key was pressed and then says
nothing until auto-repeat starts half a second later, and never says anything
at all on release. Holding W that way gave one step, a long pause, and only
then a walk -- which is what made the movement feel erratic. So the loop below
polls GetAsyncKeyState and sends the whole button mask (FF hi lo) whenever it
changes, plus a keepalive so the board's watchdog does not decide the sender
died and let go of the D-pad mid-corridor.

Only reads the keyboard while this window is in front, so typing elsewhere does
not steer Snake.

The game's console output scrolls here at the same time, filtered down to the
interesting lines.
"""
import ctypes
import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM6"
NOISE = ("[thread]", "[isr]", "[yield]", "[vbl]", "[vcd]", "[dar]",
         "[dir", "[rpk]", "I (")

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

# virtual-key code -> PSX button bit, matching keyToPadBit in esp_input.c
KEYMAP = {
    ord("W"): 1 << 12,   # UP
    ord("S"): 1 << 14,   # DOWN
    ord("A"): 1 << 15,   # LEFT
    ord("D"): 1 << 13,   # RIGHT
    ord("E"): 1 << 5,    # CIRCLE
    ord("X"): 1 << 6,    # CROSS
    ord("Q"): 1 << 4,    # TRIANGLE
    ord("Z"): 1 << 7,    # SQUARE
    ord("1"): 1 << 2,    # L1
    ord("2"): 1 << 3,    # R1
    ord("3"): 1 << 0,    # L2
    ord("4"): 1 << 1,    # R2
    0x0D: 1 << 11,       # ENTER -> START
    0x20: 1 << 8,        # SPACE -> SELECT
}
VK_ESCAPE = 0x1B

KEEPALIVE_S = 0.2        # board drops the state after 1 s of silence
POLL_S = 0.02


_console_hwnd = kernel32.GetConsoleWindow()
_focus_check_works = False


def focused():
    """True while this window should be steering Snake.

    The obvious test -- foreground window == GetConsoleWindow() -- silently
    disables every key under Windows Terminal and any other ConPTY host: the
    handle it returns belongs to a hidden pseudo-console, never to the visible
    tab, so the comparison can never succeed and nothing is ever sent. That is
    a bad failure, because the script looks like it is running fine.

    So the gate only engages once it has been seen to work at least once. On a
    classic console window that happens on the first poll and typing elsewhere
    stops reaching the game; under a ConPTY host it never does and the keys
    stay live -- which does mean ESC quits from wherever you press it.
    """
    global _focus_check_works
    if _console_hwnd and user32.GetForegroundWindow() == _console_hwnd:
        _focus_check_works = True
        return True
    return not _focus_check_works


def read_mask():
    mask = 0
    for vk, bit in KEYMAP.items():
        if user32.GetAsyncKeyState(vk) & 0x8000:
            mask |= bit
    return mask


s = serial.Serial()
s.port, s.baudrate, s.timeout = PORT, 115200, 0
# Never block on a write. The board re-enumerates its USB endpoint when it
# resets, and a blocking send during that window raises and kills the session
# mid-game; a dropped state packet costs nothing because the next keepalive
# carries the same mask.
s.write_timeout = 0.05
# hold the modem lines steady: RTS pulses reset the board on this bridge
s.dtr = False
s.rts = False
s.open()
s.dtr = True

print(f"== playing on {PORT}; ESC to quit ==")
line = b""
last_mask = -1
last_sent = 0.0
try:
    while True:
        now = time.time()
        mask = read_mask() if focused() else 0
        if focused() and user32.GetAsyncKeyState(VK_ESCAPE) & 0x8000:
            break
        if mask != last_mask or now - last_sent >= KEEPALIVE_S:
            try:
                s.write(bytes([0xFF, (mask >> 8) & 0xFF, mask & 0xFF]))
                # Echo real changes: without it there is no way to tell a key
                # that never left this script from one the board ignored.
                if mask != last_mask:
                    print("   >> %04x" % mask)
                last_mask = mask
            except serial.SerialTimeoutException:
                last_mask = -1          # resend as soon as the link recovers
            last_sent = now

        n = s.in_waiting
        if n:
            for b in s.read(n):
                if b == 0x0A:
                    text = line.decode("utf-8", "replace").rstrip()
                    if text and not any(text.startswith(p) for p in NOISE):
                        print(text)
                    line = b""
                else:
                    line += bytes([b])
        else:
            time.sleep(POLL_S)
finally:
    try:
        s.write(bytes([0xFF, 0, 0]))   # let go of everything on the way out
    except Exception:
        pass
    s.close()
    print("== end ==")
