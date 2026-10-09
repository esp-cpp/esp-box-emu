# Vendored sources

Metal Gear Solid here is not emulated: it is the FoxdieTeam decompilation of
the PlayStation game compiled natively for the ESP32-S3, following
davidmonterocrespo24's port (see
https://velxio.dev/blog/posts/metal-gear-solid-on-esp32-s3/). Three trees are
vendored; the emulator-side glue lives in `src/` and `include/`.

## `mgs/` — the game and the ESP32 port layer

- Upstream: https://github.com/FoxdieTeam/mgs_reversing (no license file; the
  decompiled game code is Konami's)
- Branch used: https://github.com/davidmonterocrespo24/mgs_reversing/tree/esp32-port
- Commit: `5a2c7ee2be477bdde465654b5bf52de1275e1096`

`mgs/source/` is the subset the ESP32 port links (`esp32/main/CMakeLists.txt`
upstream): everything except `contrib/`, `stagevr/`, `snake_vr/`, the per-stage
`stage/*.c` tables (replaced by `port/stage_union.c`, the merged table for the
stages that are 100% C) and the overlays other than `s07a s11e s11i d18ar s08br
s19br`. `mgs/port/` is the port layer (software GPU, FreeRTOS threads and
vblank, virtual CD over files, headless platform), `tools/` the disc extraction
scripts, `mgs/ESP32_README.md` and `mgs/PORT_STATUS.md` the upstream notes on
what works.

## `psyz/` — the PlayStation SDK reimplementation

- Upstream: https://github.com/Xeeynamo/sotn-decomp/tree/master/tools/psyz (MPL-2.0)
- Fork used: https://github.com/davidmonterocrespo24/psyz/tree/esp32
- Commit: `97b064c28a28d79d80655e120b336e70097328ae`

`psyz/include` (the SDK headers; unlicensed upstream), `psyz/src/psyz/*.c`
(minus `libgte_hw_sqrt.c` and `libsn.c`, which MGS supplies itself; MPL-2.0),
`psyz/src/{internal,draw}.h` and the software rasterizer
`psyz/src/platform/soft_raster.inc.c` (MPL-2.0, `psyz/src/platform/LICENSE`).

## `psyq/` — the PSY-Q library decompilation

- Upstream: the `decomp/` tree of the same psyz repository (MIT, `psyq/LICENSE`)
- Commit: `97b064c28a28d79d80655e120b336e70097328ae`

Only the translation units listed in `psyq/SOURCES.txt` (the set the SOTN ESP32
port proved to compile plus the libspu units MGS needs; `libpress/dec.c` is not
in the upstream tree) and the private headers next to them.

## Changes made for esp-box-emu

All guarded by `MGS_ESPBOX` where they sit in vendored code:

- `shim/`: the PSY-Q headers the port expected from a private directory
  (`libpad.h` constants, `mgrex.h`, `game.h`).
- The three big static buffers are pointers filled from the emulator's 4MB ROM
  block: `mgs_main_ram` (`libgv/libgv.h`, `libgv/resident.c`, `game/strctrl.c`,
  `sound/sd_main.c`, `port/psyz_port.c`), `mgs_face_group` (`port/psyz_port.c`,
  `menu/radiomes.c`) and `g_RawVram` (`port/soft_render.c`, `libdg/display.c`,
  `port/esp32_vblank.c`, `psyz/src/platform/soft_raster.inc.c`).
  `GV_ResidentAreaBottom` is computed at init instead of being a static
  initializer of the array's address.
- `port/esp32_threads.c`: per-slot stack sizes from the measured high-water
  marks; `Mgs_ThreadsPause/Resume/StopAll()`.
- `port/esp32_vblank.c`: `Mgs_PauseVblank/ResumeVblank/StopVblank()`.
- `port/virtual_cd.c`: files are read from the data root directly;
  `Mgs_CdDeinit()`.
- `port/platform_headless.c`: `Mgs_GetDataRoot()`.
- `port/psyz_port.c`: `psyz_CompMatrix()` (the name `port/include/gtemac.h`
  routes `gte_CompMatrix` to).
- `source/libsio/dummy.c`: the mts idle task's console poll yields a tick
  (`Mgs_IdleYield()`), so the emulator's own tasks on core 0 keep running.
- `psyz/src/psyz/{libcd,libsnd,libspu,psyz_spu}.c`: `libspu_private.h` is found
  through the include path instead of a `../../decomp/` relative path.
- `linker.lf`: every static of `libmgs.a` goes to PSRAM (.bss) or is bracketed
  by `_mgs_{bss,common,data}_start/_end`, which `src/mgs.cpp` uses to reset the
  game's state between launches (the PSX booted from scratch each time).
- `sound_off` and `ClearImage` are renamed with `-D` to avoid the other cores'
  globals.

## Status

What the upstream port reports (`mgs/PORT_STATUS.md`): boots into any packed
stage, Snake and the guards work, 3D with correct skeletons; no audio, no codec
calls, no cutscenes/streaming, no memory card, 8-15 fps.
