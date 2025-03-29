idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "prboom"
  PRIV_INCLUDE_DIRS "."
  REQUIRES box-emu
  )
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable -Wno-discarded-qualifiers)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=address
    -Wno-misleading-indentation
    -Wno-format-overflow
    -Wno-char-subscripts
    -Wno-missing-field-initializers
    -DHAVE_CONFIG_H
    -O2
)
