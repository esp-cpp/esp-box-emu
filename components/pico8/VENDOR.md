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
