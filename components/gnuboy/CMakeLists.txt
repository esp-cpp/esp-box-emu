idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES box-emu-hal
  )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)
target_compile_definitions(${COMPONENT_LIB} PRIVATE GNUBOY_NO_MINIZIP GNUBOY_NO_SCREENSHOT IS_LITTLE_ENDIAN)
target_compile_definitions(${COMPONENT_LIB} PUBLIC USE_GAMEBOY_GNUBOY)
