idf_component_register(
  SRC_DIRS "cpu" "libsnss" "nes" "sndhrdw" "mappers" "."
  INCLUDE_DIRS "cpu" "libsnss" "nes" "sndhrdw" "."
  REQUIRES box-emu-hal
  )
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes)
target_compile_definitions(${COMPONENT_LIB} PUBLIC USE_NES_NOFRENDO)
