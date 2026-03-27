project(libm VERSION 0.1 LANGUAGES C ASM)

set(SRC_FOLDER "src")

file(GLOB_RECURSE GENERIC_SRC
    "${SRC_FOLDER}/generic/*.c"
    "${SRC_FOLDER}/generic/*.S"
)

file(GLOB_RECURSE ARCH_SRC
    "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.c"
    "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.S"
)

set(SRC ${GENERIC_SRC} ${ARCH_SRC})

add_library(m STATIC ${SRC})

# Optional dependency hint
# target_link_libraries(m PRIVATE c)