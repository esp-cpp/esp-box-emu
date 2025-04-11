idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "fmsx" "fmsx/src/EMULib" "fmsx/src/fMSX" "fmsx/src/Z80"
  PRIV_INCLUDE_DIRS "fmsx" "fmsx/src/EMULib" "fmsx/src/fMSX" "fmsx/src/Z80"
  REQUIRES box-emu statistics shared_memory
  )
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable -Wno-discarded-qualifiers)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=stringop-truncation
    -Wno-error=format-overflow
    -Wno-implicit-fallthrough
    -Wno-format-overflow
    -Wno-stringop-truncation
    -Wno-unused-but-set-variable
    -Wno-unused-variable
    -Wno-register
    -DBPS16 -DUNIX -DLSB_FIRST -DNARROW -O2
)
file(GLOB_RECURSE c_sources "fmsx/src/*.c")
set_source_files_properties(${c_sources} PROPERTIES COMPILE_FLAGS "-include msxfix.h")
