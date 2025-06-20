cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED TARGET_ARCH)
  set(TARGET_ARCH "x86_64" CACHE STRING "Target architecture (x86_64, riscv64 or i686)")
endif()

set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos")

SET(CMAKE_C_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
SET(CMAKE_ASM_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
SET(CMAKE_CXX_COMPILER_TARGET ${TOOLCHAIN_PREFIX})

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_ASM_COMPILER "clang")
set(CMAKE_AR "llvm-ar")

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -pipe")
set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -pipe")

get_filename_component(ABSPATH "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(CMAKE_SYSROOT "${ABSPATH}/../sysroot" CACHE PATH "System root for cross-compilation")

project(bootstrapd C CXX ASM)

set(CMAKE_EXE_LINKER_FLAGS "-static")

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

add_executable(bootstrapd ${GENERIC_SRC})

set_property(TARGET bootstrapd PROPERTY C_STANDARD 23)

include(FetchContent)
FetchContent_Declare(
  libyaml
  GIT_REPOSITORY https://github.com/yaml/libyaml.git
  GIT_TAG        0.2.5
)
FetchContent_MakeAvailable(libyaml)
target_link_libraries(bootstrapd PRIVATE yaml)

