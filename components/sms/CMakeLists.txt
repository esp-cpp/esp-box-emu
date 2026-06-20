idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "smsplus" "smsplus/cpu" "smsplus/sound"
  PRIV_INCLUDE_DIRS "." "smsplus" "smsplus/cpu" "smsplus/sound"
  REQUIRES box-emu statistics shared_memory
  )
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable -Wno-discarded-qualifiers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable $<$<COMPILE_LANGUAGE:C>:-std=gnu17> $<$<COMPILE_LANGUAGE:C>:-Wno-discarded-qualifiers> $<$<COMPILE_LANGUAGE:C>:-Wno-error=address-of-packed-member>)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LSB_FIRST=1)
