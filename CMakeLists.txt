# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# add the component directories that we want to use
set(EXTRA_COMPONENT_DIRS
  "components/espp/components"
)

### NES ###
# set(NES_COMPONENTS "nofrendo nofrendo-esp32")

### GBC ###
set(GBC_COMPONENTS "gnuboy")

### SMS ###
# set(SMS_COMPONENTS "smsplus")

add_compile_definitions(BOARD_HAS_PSRAM)

set(
  COMPONENTS
  "main esptool_py esp_littlefs esp_lcd esp_psram task format display display_drivers monitor esp-idf-cxx ${NES_COMPONENTS} ${GBC_COMPONENTS} ${SMS_COMPONENTS} box-emu-hal gui"
  CACHE STRING
  "List of components to include"
  )

project(esp-box-emu)

set(CMAKE_CXX_STANDARD 20)
