idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "gwenesis/src/bus" "gwenesis/src/cpus/M68K" "gwenesis/src/cpus/Z80" "gwenesis/src/io" "gwenesis/src/savestate" "gwenesis/src/sound" "gwenesis/src/vdp"
  PRIV_INCLUDE_DIRS "." "gwenesis/src/bus" "gwenesis/src/cpus/M68K" "gwenesis/src/cpus/Z80" "gwenesis/src/io" "gwenesis/src/savestate" "gwenesis/src/sound" "gwenesis/src/vdp"
  REQUIRES box-emu statistics
  )
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable -Wno-discarded-qualifiers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable -Wno-unused-value -O3)
# target_compile_definitions(${COMPONENT_LIB} PRIVATE GWENESIS_AUDIO_ACCURATE=0)
