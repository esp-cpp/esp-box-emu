# PICO-8 for esp-box-emu, hosting femto8 (see VENDOR.md).
# COMPONENT_DIR, not CMAKE_CURRENT_SOURCE_DIR: IDF also evaluates this file in
# its early requirements pass, from the build directory.
set(F8 ${COMPONENT_DIR}/femto8/src)

file(GLOB SRC_FEMTO8 ${F8}/*.c ${F8}/lua/*.c ${F8}/lodepng/*.c ${F8}/lexaloffle/*.c)

idf_component_register(
  INCLUDE_DIRS "femto8/port/include"
  SRCS femto8/port/pico8.cpp femto8/port/platform/p8_lua_alloc.c femto8/port/platform/p8_state.c ${SRC_FEMTO8}
  PRIV_INCLUDE_DIRS "femto8/port" "femto8/port/platform" ${F8} ${F8}/data ${F8}/lua ${F8}/lodepng ${F8}/lexaloffle
  REQUIRES box-emu statistics esp_timer heap pthread
  LDFRAGMENTS "linker.lf"
  )

# Where carts keep their cartdata() saves (femto8 defaults to a relative "cdata")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
  FEMTO8_ESPBOX=1
  # the cart file buffers (256 KB) only while a cart is parsed: the box's PSRAM
  # is short, and its Lua heap needs them more
  FEMTO8_FREE_LOAD_BUFFERS=1
  # PICO8_PROFILE=1: report Lua instructions/s, GC share and the cycle count of
  # every API function with the fps line (see src/pico8.cpp)
  # PICO8_PROFILE=1
  CARTDATA_PATH="/sdcard/pico8/cdata"
  DEFAULT_CARTS_PATH="/sdcard/pico8")

target_compile_options(${COMPONENT_LIB} PRIVATE
  -O2
  -Wno-error
  -w)
