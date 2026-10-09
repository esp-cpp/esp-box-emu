# Metal Gear Solid on ESP32-S3 — port status

Last updated: 5-Aug-2026.
Board: Waveshare ESP32-S3-Touch-LCD-2 (240 MHz, 512 KB SRAM, 8 MB PSRAM
octal @120 MHz, 16 MB flash, ST7789 320x240, microSD on SPI2).

---

## Where we are

**The game boots, loads levels from the full disc and is playable.** Snake walks,
turns, swims, crosses camera volumes and collides with the world. The radar draws.
Water exists. It can boot into any of 56 stages.

What is **still missing before it can truly be called playable**:

| | status |
|---|---|
| character skeleton | fixed (strict aliasing in `CompMatrix`; see below) |
| codec calls / elevator | not working yet (see PENDING) |
| cutscenes | do not play (data missing from the card) |
| audio | not implemented |
| performance | 8-15 fps depending on scene (target 30) |
| memcard / saving | not implemented |

---

## How to build and flash

```powershell
E:\Hardware\mgs\mgs_reversing\esp32\build.ps1        # build
E:\Hardware\mgs\mgs_reversing\esp32\flash.ps1 COM6   # firmware + flash data
```

Firmware only (the usual case, much faster):

```powershell
& "C:\Espressif\python_env\idf4.4_py3.10_env\Scripts\python.exe" `
  "C:\Espressif5.5\frameworks\esp-idf-v5.5.4\components\esptool_py\esptool\esptool.py" `
  --chip esp32s3 --port COM6 --baud 921600 write_flash `
  0x10000 "E:\Hardware\mgs\mgs_reversing\esp32\build\mgs_esp32s3.bin"
```

Play with the PC keyboard:

```
python E:\Hardware\mgs\mgs_reversing\port\play.py COM6
```

W/A/S/D move, E action, X crouch, Z punch, Q first person, 1-4 triggers,
ENTER pause, ESC quit. **Close play.py before flashing**: it holds COM6.

### Choosing which level to boot into

`esp32/main/CMakeLists.txt`:

```cmake
set(MGS_START_STAGE "s01a" CACHE STRING "stage to boot into")
```

Any name from `Mgs_PlayableStages` (in `port/stage_union.c`) works, as long as the
microSD carries the full STAGE.DIR. Useful ones: `s00a` the dock (where the
game starts), `s01a` heliport (guards, snow), `s02a` tank hangar, `s03a`
armory building.

---

## Game data

It is read from a **microSD**, not from flash. Layout, at the root of the
card:

```
MGS\
  STAGE.DIR   68.6 MB   all 96 stages on the disc  <- copied
  RADIO.DAT    1.7 MB   codec                      <- copied
  FACE.DAT     3.3 MB   codec faces                <- copied
  BRF.DAT      5.5 MB   briefings                  <- copied
  DEMO.DAT     258 MB   IN-ENGINE CUTSCENES        <- PENDING
  VOX.DAT      196 MB   voices                     <- PENDING
  ZMOVIE.STR    47 MB   full-screen video          <- PENDING
```

All seven are prepared in `E:\Hardware\mgs\sdcard\MGS\`. **Copying the three
pending ones is the next concrete step**: without them the cutscenes run
empty (see "Pending" below).

**Why microSD and not flash**: reading from the card does NOT disable the
instruction cache (it is an SPI peripheral, not the XIP flash), so files are
read on demand without spending PSRAM. The flash partition only holds a
trimmed copy of 9 stages, as a fallback.

**TRAP**: when the card does not mount, the board falls back to that flash copy and
load times drop from 693 to 21 vblanks — because it reads from PSRAM. **It looks like
a 35x improvement and it is a failure in disguise.** Before trusting any load
measurement, check the log for:
`mgs: microSD mounted` and `[dir] table_size 1152 -> 96 stages`.

---

## How the port is built

It compiles the MGS decomp (`github.com/FoxdieTeam/mgs_reversing`) against **psyz**
(a reimplementation of the PSX SDK, in `E:\Hardware\sotn\sotn-decomp\tools\psyz`)
instead of the original PSY-Q, with a software rasterizer shared with the SOTN
port. All port-specific code lives under `#ifdef __psyz`.

Port-owned pieces, in `port/`:

| file | what it does |
|---|---|
| `virtual_cd.c` | virtual CD: libfs asks for absolute sectors, this translates them to files |
| `esp32_threads.c` | the mts threads on top of FreeRTOS tasks, all pinned to core 0 |
| `esp32_vblank.c` | the vblank the PSX provided + LCD scanout on core 1 |
| `soft_render.c` | links the software rasterizer to the 1 MB VRAM in PSRAM |
| `psyq_compat.c` | PSY-Q functions that psyz does not provide |
| `stage_union.c` | **generated** by `gen_stage_union.py` — single actor table |
| `platform_headless.c` | translation of `cdrom:\...` paths and files |
| `play.py` | keyboard controller over the serial port |

---

## Fixed bugs worth not stepping on again

The most expensive ones to find, each with its root cause:

**`Square0` was shifting 12 bits it should not.** The numeric suffix in PSY-Q is the
shift amount: `Square0` does NOT shift, `Square12` does. Since all of MGS
measures distances with `Square0 -> sum -> sqrt`, **every length in the game came
out divided by 64**. It broke movement, collisions, enemy range, bullets and
lighting. The clue came from the user: "the world moves fine, the player doesn't".

**Five GTE MVMVA codes copied from the wrong row** of the game's own table
(`source/include/inline_n.h`). `Psyz_GteRtv0/Rtv1/Rtv2/Ll/Llv0`
added the GTE's stale translation vector to what should have been pure
directions. Once fixed, the per-frame step went from a pinned `0,1` to a sustained `27,0`
and Snake stopped falling out of the world. **`Psyz_GteRt` IS correct — do not touch it.**

**`VSync(-1)` truncated to 16 bits** a counter the PSX returns as 32 bits. At
65,536 fields (**17.6 minutes**) the clock wrapped to zero while a task was
waiting for a larger mark, and the game froze forever. This also affects
the SOTN port.

**`PSX_W` was 256, SOTN's resolution.** MGS runs at 320x240, so the
scanout copied 256 of 320 columns and centered the crop: **one fifth of
the image lost** and a black band on each side.

**Division by zero in libhzd.** On MIPS it is harmless (undefined result, execution
continues); **on Xtensa it resets the board**. `HZD_DIV` was added and applied to
the 7 divisions with a runtime-computed divisor. General rule for this port:
any `/` with a variable divisor is a bomb.

**The LCD and the microSD share SPI2 and the driver does not arbitrate them.** The LCD is
asynchronous (one frame is 30 queued transfers), so reading the card in the
middle of an image fires an assert and resets. Fix: `Mgs_SpiBusTake/Give`, with
**draining of in-flight transfers before releasing the bus**.

**Stage scripts are TWO-PASS.** s00a is
`if (var[5] < 6) { set up the Colonel's call } else { build the dock }`, and
that call's callback does `restart`. **The world is built on the second
pass, and answering the codec is what triggers it.** Since the codec was disabled (a
mask I added myself to dodge a FACE.DAT crash), the else branch never
ran and the dock came up empty. Fix: `MENU_RadioDrainHeadless` in
`menu/radio.c`, which answers the call and dispatches its procedure without opening the
codec screen.

### The family that has cost the most: the 8-byte header

These four are the same misunderstanding with four faces, and deserve to be read
together because **there are almost certainly more left**.

The PSX packs the pointer to the next primitive and its length into ONE 32-bit
word: 24 bits of address, 8 of length. PSY-Z splits them into two
words, because a pointer on this chip does not fit in 24 bits. **Everything that
wrote, initialized or assumed the old format ended up shifted by one word.**

And there is an aggravating factor that makes it especially hard to see: on the console
`addPrim()` rewrote that shared word on every insertion, so
forgetting to set the length was **invisible**. Here "forgotten" comes to
mean "never written".

**`SetDrawStp` wrote the GP0 command on top of the length field.**
`((u_long*)p)[1] = 0xE6000000 | stp` — word 1 is the length; the command
goes in `code[0]`, word 2. The reader saw a packet of 3.8 billion
words, discarded it, and resumed **in the middle of a primitive**, reading vertices and
pointers as draw commands. It fired in combat because that function
configures semi-transparency, which is what muzzle flashes and smoke turn on.
PSY-Z had already suffered this in its own `setDrawTPage` and fixed it there; the
constructor in MGS's compatibility layer was left unreviewed.
**When PSY-Z fixes a header mismatch, sweep for the equivalents on the
game side.**

**`DG_MakePrim` did not initialize the packet length.** `GV_ZeroMemory`
only clears the `DG_PRIM` header; the packet area arrives with whatever
was in the block. Each user must write its own length and almost all of them
do. Fixed in `DG_MakePrim` itself — one place instead of chasing
every user — deriving `len = psize/4 - 2` from the size table and
checking it against the PSY-Z macros: FT4 -> 9, GT4 -> 12, TILE -> 3.

**The `DG_PrimInfos` table had the console's offsets.** First vertex at
+8 instead of +12, and every primitive 4 bytes shorter. Each transformed vertex
landed on the primitive's command word and the entire world reached
the GPU as code-0 garbage.

**The `POLY_GT4` offset**: 56 bytes, not 52, with `x0` at +12 and not at
+8. The stride between vertices does NOT change.

### A hardware rule was missing, not a line of code

**The PSX GPU REFUSES to draw polygons larger than 1023x511 pixels.**
It looks like a limitation and it is a **protection**: when a polygon crosses
behind the camera its projection overflows and the GTE pins the offending
vertices at its +/-1024 limit, producing a quad stretched from corner
to corner. The console drops them. Our rasterizer painted them whole — three
full screens of texels per frame and 142 ms of rasterization — and that
was the dock's spotlights covering everything.

Implemented in `soft_raster.inc.c` **before** clipping to the draw area:
clipping would shrink each one to screen size and hide exactly the property
being tested. Counter `psyz_tri_huge`, published as `oversize` in `[tri]`.

**Reimplementing a GPU is not just drawing what it draws: it is also NOT drawing
what it refuses to draw.**

### The compiler deleting code without warning

**`CompMatrix` lost its entire rotation to strict aliasing.** It wrote the
composed translation through a `(VECTOR*)` cast over a `long[3]`; GCC 14
at `-O2` deduced that nobody reads those writes and **eliminated the call**. The
disassembly did not contain a single multiplication: it computed `m2->t = m0->t + m1->t`.

Effect: each bone's translation was never rotated by the parent's matrix.
Each bone rotated correctly around a joint that never moved — arms
detached from the torso, knees backwards, feet pointing in different directions.

Fixed with plain `long long` arithmetic, **plus `-fno-strict-aliasing` for
the whole game**. There was at least a second identical case in the camera
computation (`libdg/display.c:318`). This is 1998 PsyQ C and does type punning
everywhere; the flag is the protection, not the one-off patch.

**When source and behavior disagree, disassemble.** `nm` for the
address, `objdump -d --start-address`, `addr2line` to confirm which
file it came from (there may be two definitions, and the linker's is the one that runs).
There is a script for this: `~/.claude/skills/retro-port-measure/scripts/`.

### Sentinel values and out-of-range accesses

**`GM_CameraList[-1]`.** The loop in `camera.c` decrements the index down to -1 when
no camera claims the change, and the following lines index the array. On
PSX that reads the preceding 16 bytes — deterministic, and the game shipped with
them. Here it is whatever the linker leaves there.

**But `-1` is NOT an error: it is the sentinel for "there is no fixed camera here"**, which
is how the game hands the shot back to the one following the player. `ChangeCamera`
handles it explicitly. My first fix treated it as a failure and pinned the previous
camera: it killed one bug and created another — the camera stayed stuck when leaving the
zone. **Before treating a value as invalid, check whether its consumer has
a branch for it.**

### Boot race

The vblank tick called `Mgs_PadsUpdate` -> `usb_serial_jtag_read_bytes`
before `PadInitDirect` had installed the driver: `LoadProhibited` in
roughly **1 of every 3 boots** (measured: 6 boots, 2 panics).
Flag `sSerialReady` in `esp32/main/esp_input.c`.

### Bounding-box culling

My own test treated two opposite cases the same way: a box that **crosses** the
near plane (meaningless projection -> draw and let the rasterizer decide) and
one that is **entirely behind** (nothing can appear -> discard). Both have
at least one corner with `z <= H/2`, so both were drawn. It is not a
rare case: an overhead shot leaves out all the floor the player has just
walked, box by box. Measured: `kept-saturated 16090 -> 4947`,
`models drawn 25093 -> 14054`.

---

## How all of this was measured

Five hypotheses died by measurement before the causes were found. None was
far-fetched; all were false. The method is distilled in the
`retro-port-measure` skill, and the essentials are:

**Design the measurement so that it CAN fail.** The `d2 == want` test compared
`|R.t|` against `|t|`, which are equal for any rotation. It could never
fail, and that is why the deformed skeleton survived for weeks behind a green
check. What did catch it: printing the positions in **two different
frames** and seeing that the parent-child offset did not change while the parent rotated
19 degrees.

**A counter that nobody increments invents a finding.** `[trap] ... 0 entered`
was on its way to becoming "the trigger system is dead"; the
counter did not have a single `++`. Once fixed, the triggers were firing from the
first second.

**Split counters by cause, not by outcome.** A "65% corrupt" was four
different things in one bucket, and three of them were normal.

**Print the two numbers you are going to compare together.** The camera was solved in
a single capture by placing its probe right next to the saturation probe, after three attempts
comparing them across different runs.

**Check where a field is WRITTEN before interpreting it.** `eye_inv.t`
looks like a position and is an already-rotated direction vector; misreading it produced a
detailed and false story about the camera flying upward.

**Packet formats vary within the same range of codes.** Three
misreadings in a row of the GPU stream, with impossible coordinates that were
PSRAM pointers. Bits: `0x10` gouraud, `0x08` quad, `0x04` texture, `0x02`
semi-transparent; rectangles (0x60-0x7F) are a whole different format.

**Do not narrow the probe on a hunch.** I filtered `[cover]` to untextured
primitives reasoning that "a flash has no texture". A light cone does
have one: I excluded exactly what I was looking for and lost a whole round.

**Correlation tells you where to look, not what to fix.**

---

## Handheld console (built and running)

XIAO ESP32S3 Sense + ILI9341 320×240 SPI display + analog stick (drone
gimbal) + 8 buttons, powered over USB. The game data lives on the Sense
board's own microSD. The console was first planned around a 3.5" ILI9488;
the panel actually fitted turned out to be an ILI9341, which also happens to
be the game's native 320×240 — no centering, two bytes per pixel.

**The complete wiring diagram is in `esp32/hardware/HARDWARE.md`** —
a schematic (`wiring.svg`), the pin table, the resistor ladder values, the
button map for both games, and the mistakes made during the build. Key
decisions it captures:

- All 11 XIAO pins in use; touch is left out (T_CS tied to 3V3,
  mandatory or it corrupts the microSD). Ladder on D2 (ADC is only available on D0-D5),
  display DC on D6.
- 6 buttons via the resistor ladder + 2 direct (the ones held down
  at the same time: fire and aim). A 74HC165 alternative was documented
  in the chat but not chosen.
- RESET on its own pin (D4): with RESET tied to 3V3 the panel did not start
  reliably. The display's GND wire is mandatory — without it the panel
  back-powers through its data pins and stays white.

**Console software (done)**: `build.ps1 xiao` — 8 MB partition table with a
3 MB app and no data partition, ILI9341 driver from the component registry,
ladder + stick sampled on their own task (never in the vblank tick), and a
non-blocking USB console. See `esp32/main/board_pins_xiao.h`.

## PENDING

### 1. The deformed skeleton — FIXED (7-Aug-2026)

Symptom: head and arms displaced, none attached to the torso, knee backwards,
feet pointing in different directions. The world looked fine; only the characters.

**Root cause: the compiler deleted a rotation.** `CompMatrix` in
`port/psyz_port.c` wrote the composed translation like this:

```c
ApplyMatrixLV(m0, (VECTOR*)&tmp.t[0], (VECTOR*)&tmp.t[0]);   /* WRONG */
```

`MATRIX::t` is `long[3]`. Writing it through a `VECTOR*` violates strict
aliasing, and GCC 14 at `-O2` deduced that nobody reads those writes and eliminated
the whole call. The disassembly of `CompMatrix` in the ELF **did not contain a single
multiplication**: it computed `m2->t = m0->t + m1->t`, a bare addition.

Effect: each bone's translation was never rotated by the parent's matrix.
Each bone rotated correctly around a joint that never moved.

Fixed with `long long` arithmetic without type punning, plus
**`-fno-strict-aliasing` for the whole game** in `esp32/main/CMakeLists.txt`.
This is 1998 PsyQ C and does punning everywhere; there was at least a
second identical case in `libdg/display.c:318` (the camera computation). The
flag is the protection, not the one-off patch.

Confirmed on hardware and by the user: *"now it looks perfect, it's properly
rigged"*.

**Why it survived weeks of measurement — the lesson worth more than the bug:**

| measurement | why it did not catch it |
|---|---|
| rest pose (M1) | with all angles at zero the rotation is the identity: rotating or not makes no difference |
| `d2 == want` | `\|R.t\| = \|t\|` for any rotation. **It is a tautology, it cannot fail.** |
| `det == 4096` | the 3x3 part was fine; the bug was only the translation |
| vertex seams | downstream symptom; measured at zero effect |

What did catch it: **printing the position of the 16 joints in two different
frames.** The parent-child offset came out identical byte for byte while
the parent's rotation changed by 19 degrees. An offset that does not move when the
parent rotates is impossible if it is being rotated.

**Design the measurement so that it CAN fail.**

**Tools left in the tree for next time:**

- `MGS_TURNTABLE` (CMake, default 0). Presents each skeleton standing and
  facing the level's camera, spinning around the screen vertical:
  `WORLD.R = EYE^T . Ry(angle)`. Since `EYE` is orthonormal its transpose is its
  inverse, so it cancels the camera and works in any level — necessary
  because s11e looks down on its room from above and a spin around the model's own axis only shows
  the top of the head. Careful: `EYE` carries the `DG_AdjustOverscan` scale, so with
  the turntable active `det` drops to ~3705 and lengths shrink by 0.4%;
  it is an artifact of the view, not of the rig, and disappears when turned off.
- Automatic rig audit in `libdg/screen.c` (`[rig]`). It checks EVERY
  skeleton the engine assembles, without needing to have it in view: `det > 0` (no
  reflections), `det` equal across all bones, and `d2/want` constant along
  the rig. **It compares the RATIO, not the value**, so it is scale
  invariant: the game scales characters legitimately and in animation (det
  4814 and 5659 were measured on the same rig in different frames, with all bones
  in agreement). A single bone that disagrees is a real defect and is printed by
  name. Silence = clean. This is what answers "are the enemies properly
  rigged?" — they use the same 16-bone rig and the same code, so they get
  audited on their own as soon as they appear on screen.

### 2. Cutscenes and the codec — blocker located

The Colonel's call when using the elevator **does happen**: the log shows
`[trap] NEW trigger 'liftcall'`, `change camera 3` and
`[codec] answered: proc ...`. It is not visible because `MENU_RadioDrainHeadless`
answers it silently, and the real codec module is turned off
(`MGS_CODEC_SCREEN` in `source/menu/menuman.c`; setting it to 1 turns it on).

**What is solved**: the 417792 bytes of the face group. The reservation
`mgs_face_group[0x80000]` in PSRAM (`port/psyz_port.c`) holds them, and
`radiomes.c` uses it when `GV_PACKET_MEMORY0` says no. **Careful**:
`menu_radio_codec_helper_helper7_80048080` must NOT pass that reservation to
`GV_FreeMemory` — freeing a static array corrupts the pool's free list far
away and much later. It is already guarded with a pointer comparison.

**What is missing, and it is a RACE**: `face data num 0`, because
`sectorAndSize` arrives as a fill pattern (`0xCCEEEEAE`). In `radiomes.c`:

    line 510:  mts_start_task(MTSID_CD_READ, menu_radio_codec_task_proc_80047AA0, ...)
    line 600:  FS_LoadFileRequest(1, startSector, size, radioDatFragment);

**The parser starts as its own task BEFORE the read request is
issued.** It reads the buffer before anyone fills it. That explains why the value
comes out byte-for-byte identical across different builds: nobody writes there.

Already ruled out (by reading code, without flashing): the file id is correct
(`cd_names[1]` is RADIO.DAT), the position table is filled correctly
(`finfo[i].pos = i * SLOT_SECTORS`), and adding the `FS_LoadFileSync` wait in the
loader did not change the value by a single bit — consistent with the race, not with a
slow read.

Next step: synchronize. Either issue the read before starting the task, or
make the task wait until the fragment is ready.

### 2b. Cutscenes run without data

`s01a` opens with the `d01a` cutscene (the helicopter one), which requests `DEMO.DAT` and
`VOX.DAT`. Since they are not on the card, it runs empty, **does not place Snake** (the
script literally prints `Where Is Snake ????`) and ends up crashing in
`DG_AllocPacks` walking uninitialized pointers.

**Fix: copy the three pending files to the microSD.** A minor
follow-up: even once they are there, the stream guard should be reviewed so that a missing
file never leaves half-built objects.

### 3. The microSD mounts intermittently

Same hardware and firmware: sometimes on the first try, sometimes all 6 attempts fail
with `ESP_ERR_TIMEOUT` or `ESP_ERR_INVALID_CRC`. `Mgs_SdCardInit` already waits 250 ms
before the first attempt and retries 6 times at 300 ms. Reseating the card
fixes it, so it points to physical contact. **40 MHz does not work on this board**
(always invalid CRC); 20 MHz.

### 4. Slow loading: 12 seconds of black screen per level

Reading 1.1 MB from the card takes ~11.5 s (≈95 KB/s, when the bus can deliver ten times
more). Raising the burst from 16 to 128 sectors gave only 7%, so that is not
the bottleneck. Skipping the redundant `fseek` on sequential reads was added
(walking the FAT of a 68 MB file on every sector), **not yet measured because the
card did not mount**. If that is not enough, look at the FATFS block size.

### 5. Performance: 8-15 fps

Dominant cost: **207 cycles per textured pixel**, almost all of it PSRAM
latency. SOTN reaches 60 fps with the same rasterizer because it is 2D and walks the
texture in a straight line (one cache read serves 128 pixels); MGS is 3D and in
a rotated triangle **every pixel is a cache miss**. The data cache is already
at its maximum (64 KB).

**The identified path**: move the hot texture pages into internal RAM,
40 times faster. It needs the ~90 KB trapped in the mts stacks, which
request 20 KB x 8 = 160 KB and use (measured with `uxTaskGetStackHighWaterMark`)
2 KB... except one task that uses **14,484 B** and where 20 KB was not excessive. The
correct sizing is that big one at 24 KB and the rest at 6 KB.

### 6. No audio, no saving

Not implemented. The SOTN port has a software SPU with 44.1 kHz pull that
could be reused.

### 7. Debug probes to remove

There are quite a few `printf` under `#ifdef __psyz` (bound.c, trans.c, screen.c,
control.c, chara.c, trap.c, wt_area.c, wt_view.c, level.c, near.c, virtual_cd.c,
esp32_vblank.c, display.c). Useful now, noise later.

### 8. 28 stages cannot be visited

Their actor tables still have raw MIPS addresses (`0x800dxxxx`) because
that code is not decompiled. `Mgs_ResolveStage` rejects them by name.
`gen_stage_union.py` regenerates the list; `s11d` is additionally excluded because it links
against an overlay that is not compiled.

---

## Method: what worked and what did not

Three lessons that were expensive to learn:

**Decode the data before suspecting the code.** The theory that "the
GCL interpreter only executes one command" was false: the block has 19 nodes and
only one prints. Four independent decodings of the retail bytecode
solved it in minutes, after two sessions of suspicion.

**Measure the board, do not reason about the code.** The skeleton investigation
ran out of suspects by reasoning, and **had never measured the hardware**. A single
measurement ruled out at once everything we had been looking at.

**A temporary stub needs something that forces it to be revisited.** Two of the three
big blockers of one session were my own shortcuts with comments that were no longer
true — the codec one said "the game plays fine without the motion
detector" and had the story engine turned off.
