cmake_minimum_required(VERSION 3.22)

set(TARGET_ARCH "riscv64" CACHE STRING "Target architecture (x86_64 or riscv64)")
set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos-")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}ar")

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(pmos-libraries VERSION 0.1)

set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -fPIC -g")
set(CMAKE_ASM_FLAGS ${CMAKE_C_FLAGS})

add_subdirectory(libc)
add_subdirectory(libpthread)
add_subdirectory(libm)
