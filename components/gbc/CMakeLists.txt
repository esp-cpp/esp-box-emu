# Game Boy / Color: gnuboy, from the esp-cpp/gnuboy submodule (the core in
# gnuboy/gnuboy, its glue in gnuboy/port)
idf_component_register(
  INCLUDE_DIRS "gnuboy/port/include"
  SRC_DIRS "gnuboy/port" "gnuboy/gnuboy/src"
  PRIV_INCLUDE_DIRS "gnuboy/gnuboy/include"
  # LDFRAGMENTS "linker.lf"
  REQUIRES "box-emu" "statistics" "shared_memory"
  )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable $<$<COMPILE_LANGUAGE:C>:-Wno-discarded-qualifiers>)
target_compile_definitions(${COMPONENT_LIB} PRIVATE GNUBOY_NO_MINIZIP GNUBOY_NO_SCREENSHOT IS_LITTLE_ENDIAN)
