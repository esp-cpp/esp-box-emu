########################################################################################################################
### GameboyCore                                                                                                      ###
########################################################################################################################

set(ENDIAN "__LITTLEENDIAN__")

set(GAMEBOYCORE_API_HEADERS
    include/gameboycore/gameboycore_api.h
    include/gameboycore/gameboycore.h
    include/gameboycore/cpu.h
    include/gameboycore/mmu.h
    include/gameboycore/gpu.h
    include/gameboycore/apu.h
    include/gameboycore/joypad.h
    include/gameboycore/link.h
    include/gameboycore/link_cable.h
    include/gameboycore/pixel.h
    include/gameboycore/sprite.h
    include/gameboycore/memorymap.h
    include/gameboycore/time.h
    include/gameboycore/instruction.h
)

set(GAMEBOYCORE_HEADERS
    ${GAMEBOYCORE_API_HEADERS}
    include/gameboycore/alu.h
    include/gameboycore/tileram.h
    include/gameboycore/tilemap.h
    include/gameboycore/oam.h
    include/gameboycore/tile.h
    include/gameboycore/timer.h
    include/gameboycore/interrupt_provider.h
    include/gameboycore/palette.h
    include/gameboycore/detail/audio/square_wave_channel.h
    include/gameboycore/detail/audio/wave_channel.h
    include/gameboycore/detail/audio/noise_channel.h
    include/gameboycore/channel.h
    include/gameboycore/memorymap.h
    include/gameboycore/mbc.h
    include/gameboycore/mbc1.h
    include/gameboycore/mbc2.h
    include/gameboycore/mbc3.h
    include/gameboycore/mbc5.h
    include/gameboycore/detail/rtc/rtc.h
    include/gameboycore/cartinfo.h
    include/gameboycore/opcodeinfo.h
    include/gameboycore/opcode_cycles.h
    src/bitutil.h
    src/shiftrotate.h
)

set(GAMEBOYCORE
    src/gameboycore.cpp
    src/cpu.cpp
    src/mmu.cpp
    src/gpu.cpp
    src/apu.cpp
    src/joypad.cpp
    src/link.cpp
    src/link_cable.cpp
    src/mbc.cpp
    src/mbc1.cpp
    src/mbc2.cpp
    src/mbc3.cpp
    src/mbc5.cpp
    src/alu.cpp
    src/cartinfo.cpp
    src/shiftrotate.cpp
    src/opcodeinfo.cpp
    src/tileram.cpp
    src/tilemap.cpp
    src/oam.cpp
    src/timer.cpp
    src/instruction.cpp
)

idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES box-emu-hal
  )

# target_compile_features(gameboycore PUBLIC cxx_std_11)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
target_compile_definitions(${COMPONENT_LIB} PRIVATE GAMEBOYCORE_STATIC=1 ${ENDIAN}=1 _CRT_SECURE_NO_WARNINGS=1)
