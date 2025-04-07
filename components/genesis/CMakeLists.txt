idf_component_register(
    SRC_DIRS
    "src"
    "gwenesis/src/bus"
    "gwenesis/src/cpus/M68K"
    "gwenesis/src/cpus/Z80"
    "gwenesis/src/sound"
    "gwenesis/src/vdp"
    "gwenesis/src/io"
    "gwenesis/src/savestate"
    INCLUDE_DIRS
        "include"
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
target_compile_definitions(${COMPONENT_LIB} PRIVATE GWENESIS_AUDIO_ACCURATE=0)
