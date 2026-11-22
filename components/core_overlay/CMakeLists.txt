# The game cores' code overlay (overlay.ld, include/core_overlay.h): the code
# of the cores that are not running stays in flash; the one a game needs is
# copied into PSRAM when it launches.
idf_component_register(
  SRCS "src/core_overlay.c" "src/store.c"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES esp_mm esp_hw_support hal spi_flash heap log esp_timer freertos)

# The flash store's size (src/store.c) from the cores' archives, after their
# code was renamed into the overlay (core_overlay_archive(), POST_BUILD)
set(cores nes gbc sms msx genesis doom darkforces pico8)
idf_build_get_property(python PYTHON)
idf_build_get_property(build_components BUILD_COMPONENTS)
set(libs "")
set(archives "")
foreach(core ${cores})
  if(core IN_LIST build_components)
    idf_component_get_property(lib ${core} COMPONENT_LIB)
    list(APPEND libs ${lib})
    list(APPEND archives "$<TARGET_FILE:${lib}>")
  endif()
endforeach()
set(size_h "${CMAKE_CURRENT_BINARY_DIR}/core_ovl_store_size.h")
add_custom_command(OUTPUT "${size_h}"
  COMMAND ${python} "${CMAKE_CURRENT_LIST_DIR}/tools/core_overlay.py" size ${CMAKE_OBJCOPY} "${size_h}" ${archives}
  DEPENDS ${libs} "${CMAKE_CURRENT_LIST_DIR}/tools/core_overlay.py"
  COMMENT "Sizing the core code overlay's flash store"
  VERBATIM)
target_sources(${COMPONENT_LIB} PRIVATE "${size_h}")
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_linker_script(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/overlay.ld")
