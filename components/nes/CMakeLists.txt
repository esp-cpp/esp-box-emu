idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "nofrendo/cpu" "nofrendo/libsnss" "nofrendo/nes" "nofrendo/sndhrdw" "nofrendo/mappers" "nofrendo"
  PRIV_INCLUDE_DIRS "nofrendo/cpu" "nofrendo/libsnss" "nofrendo/nes" "nofrendo/sndhrdw" "nofrendo"
  REQUIRES "box-emu" "statistics" "shared_memory"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable -Wno-discarded-qualifiers)
