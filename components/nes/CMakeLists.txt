idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "nofrendo/cpu" "nofrendo/libsnss" "nofrendo/nes" "nofrendo/sndhrdw" "nofrendo/mappers" "nofrendo"
  PRIV_INCLUDE_DIRS "nofrendo/cpu" "nofrendo/libsnss" "nofrendo/nes" "nofrendo/sndhrdw" "nofrendo"
  REQUIRES box-emu-hal
  )
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes)
