idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "teensysms"
  PRIV_INCLUDE_DIRS "teensysms"
  REQUIRES box-emu-hal
  )
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable -Wno-discarded-qualifiers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-pointer-arith -Wno-int-conversion -Wno-parentheses)
