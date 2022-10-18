# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# add the component directories that we want to use
set(EXTRA_COMPONENT_DIRS
  "components/espp/components"
)

set(
  COMPONENTS
  "main esptool_py esp_littlefs esp_lcd task format display display_drivers socket wifi esp-idf-cxx nes_lib box-emu-hal controller"
  CACHE STRING
  "List of components to include"
  )

project(esp-box-emu)

set(CMAKE_CXX_STANDARD 20)
