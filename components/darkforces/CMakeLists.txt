# Dark Forces for esp-box-emu, built on The Force Engine (TFE).
#
# tfe/ contains a vendored subset of BSzili's Amiga branch of TFE (based on
# TFE v1.09.410) with the __AMIGA__ platform guards renamed to TFE_ESPBOX.
# src/platform/ provides the ESP32 implementations of the platform interfaces
# (system timers, render backend, audio, threads, memory regions, stubs).
set(TFE_SRC_DIRS
  "tfe/TFE_System"
  "tfe/TFE_Memory"
  "tfe/TFE_FileSystem"
  "tfe/TFE_Archive"
  "tfe/TFE_Asset"
  "tfe/TFE_Audio"
  "tfe/TFE_Audio/MidiSynth"
  "tfe/TFE_Input"
  "tfe/TFE_Settings"
  "tfe/TFE_Game"
  "tfe/TFE_Outlaws"
  "tfe/TFE_Jedi/Collision"
  "tfe/TFE_Jedi/IMuse"
  "tfe/TFE_Jedi/InfSystem"
  "tfe/TFE_Jedi/Level"
  "tfe/TFE_Jedi/Math"
  "tfe/TFE_Jedi/Memory"
  "tfe/TFE_Jedi/Serialization"
  "tfe/TFE_Jedi/Task"
  "tfe/TFE_Jedi/Renderer"
  "tfe/TFE_Jedi/Renderer/RClassic_Fixed"
  "tfe/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed"
  "tfe/TFE_DarkForces"
  "tfe/TFE_DarkForces/Actor"
  "tfe/TFE_DarkForces/GameUI"
  "tfe/TFE_DarkForces/Landru"
  )

idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "src/platform" ${TFE_SRC_DIRS}
  PRIV_INCLUDE_DIRS "tfe" "src/platform"
  REQUIRES box-emu statistics pool_allocator esp_timer pthread heap
  LDFRAGMENTS "linker.lf"
  )

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  TFE_ESPBOX=1
  TFE_ESPBOX_BASE_DIR="/sdcard/darkforces/"
  )

target_compile_options(${COMPONENT_LIB} PRIVATE
  -Os
  -Wno-error
  -Wno-type-limits
  -Wno-ignored-qualifiers
  -Wno-unused-variable
  -Wno-unused-but-set-variable
  -Wno-unused-function
  -Wno-switch
  -Wno-sign-compare
  -Wno-missing-field-initializers
  -Wno-format
  -Wno-format-truncation
  -Wno-error=maybe-uninitialized
  -Wno-error=array-bounds
  -Wno-error=stringop-overflow
  -Wno-error=stringop-truncation
  -Wno-error=deprecated-declarations
  -Wno-error=narrowing
  -Wno-error=address
  -Wno-error=class-memaccess
  -Wno-error=reorder
  -Wno-error=parentheses
  -Wno-error=misleading-indentation
  -Wno-error=int-in-bool-context
  -Wno-error=sequence-point
  -Wno-error=strict-aliasing
  -Wno-error=char-subscripts
  -Wno-error=unused-value
  -Wno-error=implicit-fallthrough
  $<$<COMPILE_LANGUAGE:C>:-Wno-error=incompatible-pointer-types>
  $<$<COMPILE_LANGUAGE:C>:-Wno-error=discarded-qualifiers>
  )

# The renderer and the OPL3 synthesizer are the hot paths: optimize those for speed.
file(GLOB_RECURSE DF_HOT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/tfe/TFE_Jedi/Renderer/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tfe/TFE_Jedi/Math/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tfe/TFE_Audio/MidiSynth/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/tfe/TFE_Audio/MidiSynth/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tfe/TFE_Jedi/IMuse/imDigitalSound.cpp"
  )
set_source_files_properties(${DF_HOT_SOURCES} PROPERTIES COMPILE_OPTIONS "-O2")
