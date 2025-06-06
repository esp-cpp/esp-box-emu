# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.15)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# add the component directories that we want to use
set(EXTRA_COMPONENT_DIRS
  "components/espp/components"
)

# add compile definition ARDUINO_ARCH_ESP32, enabling jpegdec simd support
add_compile_definitions(ARDUINO_ARCH_ESP32)

### NES ###
set(NES_COMPONENTS "nes")

### GBC ###
set(GBC_COMPONENTS "gbc")

### SMS ###
set(SMS_COMPONENTS "sms")

### MSX ###
set(MSX_COMPONENTS "msx")

### GENESIS ###
set(GENESIS_COMPONENTS "genesis")

### DOOM ###
set(DOOM_COMPONENTS "doom")

### SNES ###
# set(SNES_COMPONENTS "snes")

add_compile_definitions(BOARD_HAS_PSRAM)

# if NES_COMPONENTS is set, add compile definitions for the NES
if(NES_COMPONENTS)
  add_compile_definitions(ENABLE_NES)
endif()

# if GBC_COMPONENTS is set, add compile definitions for the GBC
if(GBC_COMPONENTS)
  add_compile_definitions(ENABLE_GBC)
endif()

# if SMS_COMPONENTS is set, add compile definitions for the SMS
if(SMS_COMPONENTS)
  add_compile_definitions(ENABLE_SMS)
endif()

# if SNES_COMPONENTS is set, add compile definitions for the SNES
if(SNES_COMPONENTS)
  add_compile_definitions(ENABLE_SNES)
endif()

# if MSX_COMPONENTS is set, add compile definitions for the MSX
if(MSX_COMPONENTS)
  add_compile_definitions(ENABLE_MSX)
endif()

# if GENESIS_COMPONENTS is set, add compile definitions for the GENESIS
if(GENESIS_COMPONENTS)
  add_compile_definitions(ENABLE_GENESIS)
endif()

# if DOOM_COMPONENTS is set, add compile definitions for the DOOM
if(DOOM_COMPONENTS)
  add_compile_definitions(ENABLE_DOOM)
endif()

# make the components list for the emulators we want
set(EMULATOR_COMPONENTS
  ${NES_COMPONENTS}
  ${GBC_COMPONENTS}
  ${SMS_COMPONENTS}
  ${SNES_COMPONENTS}
  ${MSX_COMPONENTS}
  ${GENESIS_COMPONENTS}
  ${DOOM_COMPONENTS}
  )

set(
  COMPONENTS
  "main esptool_py esp_lcd esp_psram task format display display_drivers monitor timer ${EMULATOR_COMPONENTS} box-emu gui menu"
  CACHE STRING
  "List of components to include"
  )

project(esp-box-emu)

set(CMAKE_CXX_STANDARD 20)
