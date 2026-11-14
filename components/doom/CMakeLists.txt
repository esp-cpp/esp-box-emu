# Doom: PrBoom, from the esp-cpp/prboom submodule (the engine in
# prboom/prboom, its glue in prboom/port)
idf_component_register(
  INCLUDE_DIRS "prboom/port/include"
  SRC_DIRS "prboom/port" "prboom/prboom"
  PRIV_INCLUDE_DIRS "prboom" "prboom/prboom"
  LDFRAGMENTS "linker.lf"
  REQUIRES box-emu shared_memory
  )
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=address
    -Wno-misleading-indentation
    -Wno-format-overflow
    -Wno-char-subscripts
    $<$<COMPILE_LANGUAGE:C>:-std=gnu17>
    $<$<COMPILE_LANGUAGE:C>:-Wno-discarded-qualifiers>
    $<$<COMPILE_LANGUAGE:C>:-Wno-error=incompatible-pointer-types>
    $<$<COMPILE_LANGUAGE:C>:-Wno-error=compare-distinct-pointer-types>
    -Wno-missing-field-initializers
    -DHAVE_CONFIG_H
    -O2
)
