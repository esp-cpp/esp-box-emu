idf_component_register(
  INCLUDE_DIRS "."
  SRC_DIRS "."
  REQUIRES box-emu-hal
  )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)
target_compile_definitions(${COMPONENT_LIB} PRIVATE GNUBOY_NO_MINIZIP GNUBOY_NO_SCREENSHOT IS_LITTLE_ENDIAN)
add_definitions(-DUSE_GAMEBOY_GNUBOY)
