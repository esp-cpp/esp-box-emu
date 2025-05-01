idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "prboom"
  PRIV_INCLUDE_DIRS "." "prboom"
  REQUIRES box-emu shared_memory
  )
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=address
    -Wno-misleading-indentation
    -Wno-format-overflow
    -Wno-char-subscripts
    -Wno-missing-field-initializers
    -DHAVE_CONFIG_H
    -O2
)
