# Vendored sources

`tfe/` is a subset of **The Force Engine** (TFE), taken from BSzili's Amiga port
branch, which had already stripped the desktop-only subsystems (OpenGL, SDL,
ImGui front end, editor, mods) and added low-spec code paths.

- Upstream: https://github.com/luciusDXL/TheForceEngine (GPL-2.0)
- Branch used: https://github.com/BSzili/TheForceEngine/tree/amiga
- Commit: `d116b7a9efc29e8c9223db19d5e5ad743c8f8372` (based on TFE v1.09.410)

The file list mirrors `Makefile.amiga` from that branch (minus the Amiga
platform files under `amiga/`). Changes made for the ESP32 port:

- `__AMIGA__` platform guards were renamed to `TFE_ESPBOX` (defined by the
  component's CMakeLists).
- `TFE_System/system.cpp` and `TFE_System/log.cpp` were rewritten for
  `esp_timer` / serial logging.
- `TFE_Jedi/Renderer/virtualFramebuffer.cpp`: the 320x200 8-bit framebuffer is
  supplied by the platform (`vfb_setPlatformBuffer()`).
- `TFE_Audio/*`: MIDI devices render 16-bit integer samples (`MidiSample`),
  `midiPlayer.cpp` sleeps between iMuse updates, and the AHI left/right channel
  swap in `imDigitalSound.cpp` was removed.
- `TFE_Jedi/Task/task.cpp`: smaller task context stacks (8KB x 16 per chunk).
- `TFE_FileSystem/fileutil-posix.cpp`: no cwd / executable directory.
- `TFE_Game/saveSystem.h`: save thumbnails disabled (4x4).
- `TFE_DarkForces/darkForcesMain.cpp`: state reset restored in `exitGame()`.
- Loaders (`level.cpp`, `rtexture.cpp`, `infSystem.cpp`, `spriteAsset_Jedi.cpp`,
  `modelAsset_jedi.cpp`, `vueAsset.cpp`): `espbox_free_*_scratch()` hooks release
  the static read buffers at shutdown.
- `TFE_Asset/modelAsset_jedi.cpp`: no GPU drawId free after the level region is cleared.
- `TFE_Game/saveSystem.cpp`: absolute save paths; buffers nulled on shutdown.
- `TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixedSharedState.*`: the state is a
  platform allocated pointer instead of a 320KB static.
- Large statics (stream/parser/settings/save buffers, OPL3 chip, MIDI buffers,
  palettes, ...) are pointers allocated from BoxEmu's 4MB ROM block through
  per-file `espbox_shared_*()` hooks (`TFE_System/espboxShared.h`).
- Shutdown-only `espbox_release_*_caches()` hooks (settings, paths, MIDI player,
  renderer, 3D object, texture, sprite and model caches) release static STL
  container storage so nothing is left in the ROM block between sessions.
- `TFE_DarkForces/sound.cpp`: level sounds are registered at startup but read
  from the card the first time they play (`vocFileExists()` in `lsound.cpp`).
- `TFE_DarkForces/GameUI/escapeMenu.cpp`: the escape / confirmation menu frames
  are loaded when the menu opens and freed when it closes.
- `TFE_Archive/gobArchive.cpp`: directory reads are validated.
- `TFE_Audio/MidiSynth/opl3.c`: lookup tables placed in internal RAM (`DRAM_ATTR`).

All allocations made by this library go to the 4MB ROM block while the game
runs: see `../src/platform/esp_alloc.cpp` and `../alloc_redirect.syms`.

`SDL_endian.h` is the Amiga branch's little-endian shim for `TFE_System/endian.h`.

The ESP32 platform implementations live in `../src/platform/`.
