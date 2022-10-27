set(ENDIAN "__LITTLEENDIAN__")

idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES box-emu-hal
  )

# target_compile_features(gameboycore PUBLIC cxx_std_11)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
target_compile_definitions(${COMPONENT_LIB} PRIVATE GAMEBOYCORE_STATIC=1 ${ENDIAN}=1 _CRT_SECURE_NO_WARNINGS=1)
target_compile_definitions(${COMPONENT_LIB} PUBLIC USE_GAMEBOY_GAMEBOYCORE)
