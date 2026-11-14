# NES: nofrendo, from the esp-cpp/nofrendo submodule (the core in
# nofrendo/nofrendo, its glue in nofrendo/port)
set(NF "nofrendo/nofrendo")
idf_component_register(
  INCLUDE_DIRS "nofrendo/port/include"
  SRC_DIRS "nofrendo/port" "${NF}/cpu" "${NF}/libsnss" "${NF}/nes" "${NF}/sndhrdw" "${NF}/mappers" "${NF}"
  PRIV_INCLUDE_DIRS "${NF}/cpu" "${NF}/libsnss" "${NF}/nes" "${NF}/sndhrdw" "${NF}"
  REQUIRES "box-emu" "statistics" "shared_memory"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable $<$<COMPILE_LANGUAGE:C>:-std=gnu17> $<$<COMPILE_LANGUAGE:C>:-Wno-discarded-qualifiers>)
