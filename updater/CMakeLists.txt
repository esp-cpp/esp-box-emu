# The ESP-Box-Emu updater: a small app in the ota_1 slot (the emulator cannot
# write its own flash, see partitions.csv). An update reboots into it (the emulator's
# test control ENTER_UPDATER request); it takes the new emulator image over
# the USB link (espp OTA service), writes ota_0 and boots it. With nothing to
# do it boots the emulator again after a while. See tools/emulink.
cmake_minimum_required(VERSION 3.20)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(COMPONENTS main esptool_py CACHE STRING "List of components to include" FORCE)

project(esp-box-emu-updater)

set(CMAKE_CXX_STANDARD 23)
