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

# The globals another core in this firmware defines too, renamed in this
# library (its glue included). PatchZ80: the Genesis core's Z80 has a stub of
# that name, and the linker resolved fMSX's call (the ED FE disk BIOS patch
# trap) to it, so fMSX's own (fMSX/Patch.c) was never linked.
add_custom_command(TARGET ${COMPONENT_LIB} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} --redefine-syms=${CMAKE_CURRENT_LIST_DIR}/symbol_renames.syms $<TARGET_FILE:${COMPONENT_LIB}>
  COMMENT "Renaming the globals another core also defines"
  VERBATIM)
core_overlay_rebuild_on(${CMAKE_CURRENT_LIST_DIR}/symbol_renames.syms)

# Its code goes into the code overlay (components/core_overlay): in PSRAM only
# while one of its games runs
core_overlay_archive(msx)
