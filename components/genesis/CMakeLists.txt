idf_component_register(
    SRC_DIRS
    "gwenesis/port"
    "gwenesis/src/bus"
    "gwenesis/src/cpus/M68K"
    "gwenesis/src/cpus/Z80"
    "gwenesis/src/sound"
    "gwenesis/src/vdp"
    "gwenesis/src/io"
    "gwenesis/src/savestate"
    INCLUDE_DIRS
        "gwenesis/port/include"
        "gwenesis/src"
        "gwenesis/src/bus"
        "gwenesis/src/cpus/M68K"
        "gwenesis/src/cpus/Z80"
        "gwenesis/src/sound"
        "gwenesis/src/vdp"
        "gwenesis/src/io"
        "gwenesis/src/savestate"
    REQUIRES
        shared_memory
        box-emu
)
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-char-subscripts -Wno-attributes -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable -Wno-discarded-qualifiers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable -Wno-unused-value -Ofast)
# target_compile_definitions(${COMPONENT_LIB} PRIVATE GWENESIS_AUDIO_ACCURATE=0 BUILD_TABLES=0 TABLES_FULL=0)
# M68K_OVERCLOCK_SHIFT: the 68000 clock setting (genesis_set_cpu_percent());
# M68K_IDLE_SKIP: skip the 68000's wait-for-frame loops (see m68k_run())
target_compile_definitions(${COMPONENT_LIB} PRIVATE GWENESIS_AUDIO_ACCURATE=0 M68K_OVERCLOCK_SHIFT=8 M68K_IDLE_SKIP)
