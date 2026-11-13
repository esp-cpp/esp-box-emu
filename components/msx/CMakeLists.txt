# MSX: fMSX, from the esp-cpp/fmsx submodule (the core in fmsx/fmsx, its
# glue in fmsx/port)
set(FM "fmsx/fmsx")
idf_component_register(
  INCLUDE_DIRS "fmsx/port/include"
  SRC_DIRS "fmsx/port" "${FM}" "${FM}/src/EMULib" "${FM}/src/fMSX" "${FM}/src/Z80"
  PRIV_INCLUDE_DIRS "${FM}" "${FM}/src/EMULib" "${FM}/src/fMSX" "${FM}/src/Z80"
  LDFRAGMENTS "linker.lf"
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
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>
    -DBPS16 -DUNIX -DLSB_FIRST -DNARROW -O2
)
file(GLOB_RECURSE c_sources "${CMAKE_CURRENT_SOURCE_DIR}/${FM}/src/*.c")
set_source_files_properties(${c_sources} PROPERTIES COMPILE_FLAGS "-include msxfix.h")
