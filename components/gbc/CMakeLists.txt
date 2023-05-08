idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "gnuboy/src"
  PRIV_INCLUDE_DIRS "gnuboy/include"
  REQUIRES box-emu-hal
  )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)
target_compile_definitions(${COMPONENT_LIB} PRIVATE GNUBOY_NO_MINIZIP GNUBOY_NO_SCREENSHOT IS_LITTLE_ENDIAN)
