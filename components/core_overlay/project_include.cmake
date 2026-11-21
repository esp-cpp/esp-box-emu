# core_overlay_archive(<core> [keep-regex ...]): in a game core's component
# CMakeLists.txt, after idf_component_register(): put the core's code into the
# code overlay (overlay.ld; tools/core_overlay.py rename, POST_BUILD of the
# component's library). keep-regex: code that stays resident although it is in
# the core's archive (matched against the function name; see the script).
set(CORE_OVERLAY_TOOL "${CMAKE_CURRENT_LIST_DIR}/tools/core_overlay.py")

function(core_overlay_archive core)
  idf_build_get_property(python PYTHON)
  set(keep "")
  foreach(k ${ARGN})
    list(APPEND keep "--keep=${k}")
  endforeach()
  add_custom_command(TARGET ${COMPONENT_LIB} POST_BUILD
    COMMAND ${python} ${CORE_OVERLAY_TOOL} rename ${CMAKE_OBJCOPY} $<TARGET_FILE:${COMPONENT_LIB}> ${core} ${keep}
    COMMENT "Moving the ${core} core's code into the code overlay"
    VERBATIM)
endfunction()
