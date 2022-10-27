idf_component_register(
  SRC_DIRS "src"
  INCLUDE_DIRS "include"
  REQUIRES box-emu-hal
  )
target_compile_definitions(${COMPONENT_LIB} PUBLIC USE_NES_NES_LIB)
