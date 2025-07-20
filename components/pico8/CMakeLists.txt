idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "femto8/src"
  PRIV_INCLUDE_DIRS "femto8/include"
  REQUIRES "box-emu" "statistics" "shared_memory"
)

target_compile_options(${COMPONENT_LIB} PRIVATE 
  -Wno-unused-function 
  -Wno-unused-variable
  -Wno-implicit-fallthrough
  -Wno-discarded-qualifiers
  -DFEMTO8_EMBEDDED
  -DFEMTO8_NO_FILESYSTEM
  -DFEMTO8_ESP32
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE 
  PICO8_EMULATOR
  FEMTO8_EMBEDDED
)