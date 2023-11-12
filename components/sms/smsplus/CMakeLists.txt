idf_component_register(
  INCLUDE_DIRS "." "cpu" "sound"
  SRC_DIRS "." "cpu" "sound"
  REQUIRES box-emu-hal
  )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-misleading-indentation)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LSB_FIRST=1)
target_compile_definitions(${COMPONENT_LIB} PUBLIC USE_SMS_SMSPLUS)
