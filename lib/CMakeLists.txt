cmake_minimum_required(VERSION 3.22)

if (NOT DEFINED TARGET_ARCH)
    set(TARGET_ARCH "x86_64" CACHE STRING "Target architecture (x86_64 or riscv64)")
endif()
set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos")

set(CMAKE_C_COMPILER "clang")
set(CMAKE_ASM_COMPILER "clang")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}-ar")

SET(CMAKE_C_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
SET(CMAKE_ASM_COMPILER_TARGET ${TOOLCHAIN_PREFIX})

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(pmos-libraries VERSION 0.1)

get_filename_component(ABSPATH "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(SYSROOT "${ABSPATH}/../sysroot")

set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -g -nostdlib -fPIC --sysroot=${SYSROOT}")
set(CMAKE_ASM_FLAGS ${CMAKE_C_FLAGS})

add_subdirectory(libc)
add_subdirectory(libpthread)
add_subdirectory(libm)
