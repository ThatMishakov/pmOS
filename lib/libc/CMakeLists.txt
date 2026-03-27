project(libc VERSION 0.1 LANGUAGES C ASM)

set(SRC_FOLDER "src")

# ========================
# Sources
# ========================

file(GLOB_RECURSE GENERIC_SRC
    "${SRC_FOLDER}/generic/*.c"
    "${SRC_FOLDER}/generic/*.S"
)

file(GLOB_RECURSE ARCH_SRC
    "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.c"
    "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.S"
)

# Remove crt files from main libc build
list(FILTER ARCH_SRC EXCLUDE REGEX ".*/crti\\.S$")
list(FILTER ARCH_SRC EXCLUDE REGEX ".*/crtn\\.S$")
list(FILTER ARCH_SRC EXCLUDE REGEX ".*/crt0\\.S$")

set(SRC ${GENERIC_SRC} ${ARCH_SRC})

# ========================
# GCC/Clang builtin fixes
# ========================

set_source_files_properties(
    ${SRC_FOLDER}/generic/malloc/malloc.c
    PROPERTIES COMPILE_FLAGS
    "-fno-builtin-malloc -fno-builtin-free -fno-builtin-calloc -fno-builtin-realloc"
)

set_source_files_properties(
    ${SRC_FOLDER}/generic/string.c
    PROPERTIES COMPILE_FLAGS "-fno-builtin"
)

# ========================
# libc.a
# ========================

add_library(csrc OBJECT ${SRC})
add_library(c STATIC $<TARGET_OBJECTS:csrc>)

# ========================
# CRT objects (startup files)
# ========================

add_library(crtstuff OBJECT
    ${SRC_FOLDER}/arch/${TARGET_ARCH}/crti.S
    ${SRC_FOLDER}/arch/${TARGET_ARCH}/crtn.S
    ${SRC_FOLDER}/arch/${TARGET_ARCH}/crt0.S
)

# ========================
# Install
# ========================

# libc archive handled by top-level install()

# Install crt objects (important for toolchain!)
install(FILES
    $<TARGET_OBJECTS:crtstuff>
    DESTINATION "/usr/lib"
)