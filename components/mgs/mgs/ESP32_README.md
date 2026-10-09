# Metal Gear Solid on the ESP32-S3

<a href="https://youtube.com/shorts/XTUJKQWMhi0"><img src="../docs/esp32/gameplay.gif" alt="Gameplay on the XIAO ESP32S3 handheld" width="320"></a>

Gameplay on the handheld: [watch it on YouTube](https://youtube.com/shorts/XTUJKQWMhi0).

A native port — not an emulator — of the Metal Gear Solid decompilation to an
ESP32-S3 microcontroller: 240 MHz dual-core Xtensa, 512 KB of internal SRAM,
8 MB of PSRAM. The whole engine is compiled to Xtensa machine code; the
PlayStation SDK is replaced by [psyz](https://github.com/davidmonterocrespo24/psyz)
(including the GTE geometry macros MGS needs) and a software rasterizer.

Two targets:

| | Waveshare ESP32-S3-Touch-LCD-2 | DIY handheld (XIAO ESP32S3 Sense) |
|---|---|---|
| Build | `build.ps1` | `build.ps1 xiao` |
| Panel | ST7789 | ILI9341, 320×240 native |
| Game data | FAT partition in flash (trimmed `STAGE.DIR`) or microSD | microSD (full 71 MB `STAGE.DIR`) |
| Frame rate | ~15 fps in the dock with enemies | similar |

The handheld's wiring, parts list and button map are in
[`hardware/HARDWARE.md`](hardware/HARDWARE.md). It is the same console that
runs the Castlevania: Symphony of the Night port.

## You need your own disc

Nothing derived from the game is in this repository. Extract the files from
**your own** disc image with `port/extract_disc.py` and copy them to the
microSD under `/MGS/` (`STAGE.DIR`, `RADIO.DAT`, …). For the Waveshare board's
8 MB flash partition, `port/pack_stagedir.py` rebuilds a `STAGE.DIR` holding
only the stages that fit.

## Status

**Works:** boot straight into gameplay at the dock (`s00a`); Snake, guards,
searchlights, cameras, alerts and gunfire; 3D models with correct skeletons;
no panics over long play sessions.

**Not yet:**
- **Codec calls** (radio dialogues). The data reaches the parser but the index
  into `RADIO.DAT` is wrong; see [`port/STATUS.md`](../port/STATUS.md).
- **The opening** (swim-in and briefing) needs memory card, CD streaming and
  the codec together.
- **The elevator**: its actor is never requested by the stage script —
  most likely gated behind story progress that only advances through a codec
  call.
- CD audio/video streaming; performance beyond ~15 fps.

## Bugs worth knowing about

This port shook out several problems that a PlayStation silently tolerates and
modern compilers or real memory protection do not — the full journal is
[`port/STATUS.md`](../port/STATUS.md):

- **Strict aliasing deleted a whole matrix rotation.** 1998 PSY-Q C type-puns
  freely; GCC 14 at `-O2` removed stores in `CompMatrix` it "proved" dead, and
  every skeleton came out deformed. Found by disassembly: the function had zero
  multiply instructions. `-fno-strict-aliasing` fixes the class.
- **The PSX GPU refuses polygons larger than 1023×511**, and the game relies on
  it: projections behind the camera saturate at the GTE clamp and produce
  screen-sized quads that the console simply drops. A software rasterizer draws
  them — as a white sheet over the screen during alerts.
- **The OT tag is two words in psyz, one on the PSX.** `SetDrawStp` and
  `DG_MakePrim` wrote the packed PSX layout into the length field.
- **`printf` over native USB blocks when nobody is listening**, which froze the
  cooperative task scheduler inside the game's own debug output.

## Building

ESP-IDF v5.5, plus a checkout of the companion SOTN port, which provides psyz
(the PlayStation SDK reimplementation both games share) and a few headers:

```
git clone -b esp32-port --recurse-submodules https://github.com/davidmonterocrespo24/sotn-decomp
```

Point the build at it with the `SOTN` CMake cache variable
(`esp32/main/CMakeLists.txt`) or the `SOTN` environment variable for
`port/build.sh`. Then:

```
powershell -File esp32/build.ps1 xiao
powershell -File esp32/flash.ps1 COM9 -Xiao
```
