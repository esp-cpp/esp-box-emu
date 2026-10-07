# Vendored sources

`femto8/` is **femto8**, Ben Baker's PICO-8 reimplementation for embedded
systems (C, a fixed-point Lua 5.x fork, lodepng for `.p8.png` carts and the
Lexaloffle `.p8.png` code decompressor).

- Upstream: https://github.com/benbaker76/femto8 (MIT, `femto8/LICENSE`)
- Commit: `f9e5097c882d0c9203616689f3de5531e0597c43`
- Bundled: `src/lua` (Lua, MIT), `src/lodepng` (zlib, `lodepng/LICENSE`),
  `src/lexaloffle` (Lexaloffle Games, zlib-style notice in the files)

Everything under `femto8/src/` except `main.c` (the desktop/SDL entry point).

## Changes made for esp-box-emu

A third platform, `FEMTO8_ESPBOX`, next to upstream's SDL and Renesas
DA1470x (`__DA1470x__`) targets. The emulator supplies video, audio and input
through `src/platform/p8_espbox.h`, implemented in `src/pico8.cpp`:

- `p8_emu.h`: `FEMTO8_ESPBOX` selects `OS_FREERTOS` (FreeRTOS clock/sleep)
  and `ENABLE_AUDIO`; the DA1470x heap redirection no longer applies to every
  FreeRTOS target.
- `p8_emu.c`: FreeRTOS headers from `freertos/`; the generic renderer (the one
  the SDL build uses, with the screen transforms and high-colour modes) also
  serves the box, writing 5-bit palette indices into a frame the emulator
  provides (`p8_espbox_frame_begin/end`) instead of ARGB pixels; no on-screen
  fps counter; the LCD controller and DA1470x renderer are guarded.
- `p8_input.c`: buttons come from `p8_espbox_buttons()`; `p8_pump_events()`
  (femto8's Lua instruction hook) calls `p8_espbox_pump()`, where the emulator
  menu pauses the cart and a quit from the menu unwinds it through `p8_quit()`.
- `linker.lf`: femto8's statics go to PSRAM and are bracketed by SURROUND
  symbols so `src/pico8.cpp` can reset them between launches (femto8 was
  written to run once per process).
- `CARTDATA_PATH` / `DEFAULT_CARTS_PATH` are set to `/sdcard/pico8/...` at
  build time.
- PICO-8 compatibility (also fixed in the desktop build, which is the
  reference used to verify them): a cart that overrides `_ENV`
  (`for _ENV in all(objs) do circfill(...) end`, `function f(_ENV)`) still
  reaches the API through a fallback in `lua/lvm.c` (the same approach as
  fake-08's z8lua); `split()` accepts a non-string; `load("#bbs_id")`
  resolves to a sibling `bbs_id.p8.png` / `.p8` so multi-cart games work when
  their carts sit next to each other; `load()` resolves against the cart's
  own folder (no working directory on the ESP-IDF VFS); `p8_wait_for_any_key`
  accepts a gamepad button.
- Performance (`p8_lua_helper.h`): `sspr()` only walks the destination pixels
  inside the clip rectangle (a cart scaling a sprite to thousands of pixels
  looked hung); `circfill`/`ovalfill` draw each scanline once; `draw_hline`
  writes screen nibbles directly in the common case; `cls` is a memset. All
  verified pixel-identical against the previous code with a test cart.
- `p8_lua.c`: the Lua state uses the pooled allocator in
  `src/platform/p8_lua_alloc.c` (small objects from internal SRAM first, then
  PSRAM); `lvm`/`ltable`/`lstring` run from IRAM (`linker.lf`); a
  `PICO8_PROFILE` build wraps every API function with a cycle counter.

## Status and limits

Measured on the BOX-3: the interpreter executes ~1.3M Lua instructions/s
with its heap in PSRAM (GC ~1%, the drawing API ~5-15% of a frame), which
runs Celeste at its full 30 fps but leaves CPU-heavy carts near PICO-8's
limit (e.g. Cattle Crisis, Mossmoss) at a few fps. Known: Pico Ball shows a
blank screen in upstream femto8 as well; no mouse/keyboard.

## Save states

`src/platform/p8_state.c`: PICO-8 RAM, the overlay, the frame counters, the
audio channels and the cart's Lua heap. The heap is serialized with **Eris**
(`femto8/src/lua/eris.c`, MIT, Florian Nuecke; the Lua 5.2.4 version as
carried in fake-08's z8lua, with the fixed-point number I/O and the
`populateperms` helper for this Lua). Everything the cart created or
replaced in `_G` is persisted as a graph; the API functions and library
tables recorded before the cart ran (`p8_state_init`) are permanents
referenced by name, as are the registry, `_G` and the main thread. The
menu's request is carried out by the cart task at its next frame boundary
(`p8_espbox_frame_boundary()` in `p8_main_loop`), where no cart code is on
the Lua stack; the cart's own `cartdata()` file is untouched.
