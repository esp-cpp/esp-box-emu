# Metal Gear Solid for esp-box-emu.
#
# A native build of the FoxdieTeam decompilation (mgs_reversing), using the
# ESP32-S3 port by davidmonterocrespo24: the PlayStation SDK is replaced by psyz
# (a C reimplementation with a software GPU rasterizer and the GTE) plus the
# PSY-Q library decompilation, and the PSX kernel's threads and vblank run on
# FreeRTOS. See VENDOR.md for what is vendored and what was changed.
#
# Only one stage can be linked statically (every file in source/stage/ defines
# its own _StageCharacterEntries[]); port/stage_union.c is the merged table
# the ESP32 port generates for all the stages that can be packed.
# COMPONENT_DIR, not CMAKE_CURRENT_SOURCE_DIR: IDF also evaluates this file in
# its early requirements pass, from the build directory
set(MGS_ROOT ${COMPONENT_DIR}/mgs)
set(PSYZ_ROOT ${COMPONENT_DIR}/psyz)
set(PSYQ_ROOT ${COMPONENT_DIR}/psyq)

# Where the game drops you when it starts (any stage of port/stage_union.c
# works, given the full STAGE.DIR). s00a is the dock, where the game begins;
# "title" is the game's own boot chain, which the upstream port cannot run yet
# (memory card and CD streaming are missing).
set(MGS_START_STAGE "s00a" CACHE STRING "stage Metal Gear Solid boots into")

# ---- the game (overlays and stage tables are curated by the vendoring) ----
file(GLOB_RECURSE SRC_GAME ${MGS_ROOT}/source/*.c)
list(APPEND SRC_GAME ${MGS_ROOT}/port/stage_union.c)

# ---- the port layer: PSX kernel / SDK pieces on FreeRTOS -------------------
set(SRC_PORT
  ${MGS_ROOT}/port/psyz_port.c
  ${MGS_ROOT}/port/psyq_compat.c
  ${MGS_ROOT}/port/soft_render.c
  ${MGS_ROOT}/port/platform_headless.c
  ${MGS_ROOT}/port/stage_stubs.c
  ${MGS_ROOT}/port/esp32_threads.c
  ${MGS_ROOT}/port/esp32_vblank.c
  ${MGS_ROOT}/port/virtual_cd.c)

# ---- psyz (minus the pieces MGS supplies itself) ---------------------------
file(GLOB SRC_PSYZ ${PSYZ_ROOT}/src/psyz/*.c)

# ---- the PSY-Q decompilation subset (psyq/SOURCES.txt) ----------------------
file(STRINGS ${PSYQ_ROOT}/SOURCES.txt PSYQ_FILES)
set(SRC_PSYQ "")
foreach(f ${PSYQ_FILES})
  list(APPEND SRC_PSYQ ${PSYQ_ROOT}/${f})
endforeach()

# ---- esp-box-emu glue -------------------------------------------------------
set(SRC_GLUE
  src/mgs.cpp
  src/platform/mgs_input.c
  src/platform/mgs_present.c)

idf_component_register(
  INCLUDE_DIRS "include"
  SRCS ${SRC_GLUE} ${SRC_GAME} ${SRC_PORT} ${SRC_PSYZ} ${SRC_PSYQ}
  PRIV_INCLUDE_DIRS
    "src/platform"
    "${MGS_ROOT}/source"          # MUST precede psyz: both have a common.h
    "${MGS_ROOT}/source/include"
    "shim"
    "${PSYZ_ROOT}/include"
    "${PSYZ_ROOT}/src"
    "${PSYQ_ROOT}/src"               # psyz's libspu/libspu_private.h
    "${MGS_ROOT}/port/include"
  REQUIRES box-emu statistics pool_allocator esp_timer heap esp_system
  LDFRAGMENTS "linker.lf"
  )

# the game's build configuration: C only (INTEGRAL clashes with a template
# parameter in libfmt, which the C++ glue uses)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
  $<$<COMPILE_LANGUAGE:C>:INTEGRAL>
  $<$<COMPILE_LANGUAGE:C>:__psyz>
  $<$<COMPILE_LANGUAGE:C>:VERSION_PC>
  $<$<COMPILE_LANGUAGE:C>:NON_MATCHING>
  $<$<COMPILE_LANGUAGE:C>:PERMUTER>
  $<$<COMPILE_LANGUAGE:C>:HARD_LINK>
  $<$<COMPILE_LANGUAGE:C>:_internal_version_us>
  MGS_ESPBOX=1
  MGS_START_STAGE="${MGS_START_STAGE}")

# 1998 PSY-Q C: gnu17 (GCC 14's gnu23 default rejects it), int/long/pointer
# mixing that was harmless on MIPS, and type punning everywhere (strict aliasing
# removed the stores of CompMatrix and detached every limb from every model).
# -O2: the rasterizer and the GTE are the frame budget.
target_compile_options(${COMPONENT_LIB} PRIVATE
  -O2
  -w
  -Wno-error
  -fno-strict-aliasing
  $<$<COMPILE_LANGUAGE:C>:-std=gnu17>
  $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-pointer-types>
  $<$<COMPILE_LANGUAGE:C>:-Wno-int-conversion>
  $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>
  $<$<COMPILE_LANGUAGE:C>:-Wno-builtin-declaration-mismatch>
  $<$<COMPILE_LANGUAGE:C>:-Wno-return-mismatch>
  # SHELL: keeps CMake from merging the repeated -include flags
  "$<$<COMPILE_LANGUAGE:C>:SHELL:-include string.h>"
  "$<$<COMPILE_LANGUAGE:C>:SHELL:-include gtemac.h>"
  # MGS_WHERE breadcrumbs (the printf redirection inside is for another board)
  "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${MGS_ROOT}/port/mgs_printf.h>")

# Global names the game shares with other emulator cores in this firmware
# (gnuboy's sound_off, fMSX's ClearImage); rename them throughout libmgs.a.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
  $<$<COMPILE_LANGUAGE:C>:sound_off=mgs_sound_off>
  $<$<COMPILE_LANGUAGE:C>:ClearImage=mgs_ClearImage>)

# The PSY-Q decompilation's <common.h> is psyz's (INCLUDE_ASM compiles to
# nothing there); the game's own common.h shadows it on the include path, so
# force psyz's in first for these files.
set_source_files_properties(${SRC_PSYQ} PROPERTIES
  COMPILE_OPTIONS "-include;${PSYZ_ROOT}/include/common.h")

# the game's main() is started from the emulator (src/mgs.cpp)
set_source_files_properties(${MGS_ROOT}/source/main/main.c PROPERTIES
  COMPILE_DEFINITIONS "main=mgs_main")
