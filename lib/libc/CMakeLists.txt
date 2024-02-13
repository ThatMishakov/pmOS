cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 17)

set(TARGET_ARCH "x86_64" CACHE STRING "Target architecture (x86_64 or riscv64)")
set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos-")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}ar")

project(libc VERSION 0.1 LANGUAGES C ASM)

set(CMAKE_C_FLAGS "-Wall -Wextra -O2" CACHE STRING "C compiler flags")
set(CMAKE_ASM_FLAGS ${CMAKE_C_FLAGS} CACHE STRING "ASM compiler flags")

set(SRC_FOLDER "src")

file(GLOB_RECURSE GENERIC_SRC "${SRC_FOLDER}/generic/*.c" "${SRC_FOLDER}/generic/*.S")
file(GLOB_RECURSE ARCH_SRC "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.c" "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.S")

add_library(libc STATIC ${GENERIC_SRC} ${ARCH_SRC})
set_target_properties(libc PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sysroot/usr/lib" CACHE PATH "Installation directory")
install(TARGETS libc ARCHIVE DESTINATION ${INSTALL_DIR}/lib)