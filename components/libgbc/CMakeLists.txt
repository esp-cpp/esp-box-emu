idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES box-emu-hal
  )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -O2 -Ofast)
# target_compile_definitions(${COMPONENT_LIB} PRIVATE GAMEBRO_INDEXED_FRAME)
target_compile_definitions(${COMPONENT_LIB} PUBLIC USE_GAMEBOY_LIBGBC)
