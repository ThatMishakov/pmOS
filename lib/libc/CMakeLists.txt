cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 17)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(CMAKE_ASM_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)


project(libc VERSION 0.1 LANGUAGES C ASM)

set(SRC_FOLDER "src")

file(GLOB_RECURSE GENERIC_SRC "${SRC_FOLDER}/generic/*.c" "${SRC_FOLDER}/generic/*.S")
file(GLOB_RECURSE ARCH_SRC "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.c" "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.S")

list(FILTER ARCH_SRC EXCLUDE REGEX ".*/crti\.S$")
list(FILTER ARCH_SRC EXCLUDE REGEX ".*/crtn\.S$")
list(FILTER ARCH_SRC EXCLUDE REGEX ".*/crt0\.S$")

set(SRC ${GENERIC_SRC} ${ARCH_SRC})

# GCC shenanigans
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67618
# https://github.com/zephyrproject-rtos/zephyr/issues/64941
# https://github.com/zephyrproject-rtos/zephyr/pull/65082
# https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
#
# Apply -fno-inline-malloc so GCC doesn't optimize calloc to itself, creating infinite recursion
set_source_files_properties(${SRC_FOLDER}/generic/malloc/malloc.c PROPERTIES COMPILE_FLAGS "-fno-builtin-malloc -fno-builtin-free -fno-builtin-calloc -fno-builtin-realloc")
set_source_files_properties(${SRC_FOLDER}/generic/string.c PROPERTIES COMPILE_FLAGS "-fno-builtin")

add_library(csrc OBJECT ${SRC})
add_library(c STATIC $<TARGET_OBJECTS:csrc>)
# add_library(c_static SHARED $<TARGET_OBJECTS:csrc>)

# set_target_properties(c_static PROPERTIES OUTPUT_NAME c)

# crti and crtn
add_library(crtstuff OBJECT ${SRC_FOLDER}/arch/${TARGET_ARCH}/crti.S ${SRC_FOLDER}/arch/${TARGET_ARCH}/crtn.S ${SRC_FOLDER}/arch/${TARGET_ARCH}/crt0.S)

set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sysroot/usr/lib" CACHE PATH "Installation directory")
install(TARGETS c ARCHIVE DESTINATION ${INSTALL_DIR})
# install(TARGETS c_static DESTINATION ${INSTALL_DIR})

install(FILES $<TARGET_OBJECTS:crtstuff> DESTINATION ${INSTALL_DIR})