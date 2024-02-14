cmake_minimum_required(VERSION 3.22)

set(TARGET_ARCH "x86_64" CACHE STRING "Target architecture (x86_64 or riscv64)")
set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos-")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}ar")

project(pmos-libraries VERSION 0.1)

add_subdirectory(libc)
add_subdirectory(libpthread)
add_subdirectory(libm)