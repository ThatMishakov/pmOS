cmake_minimum_required(VERSION 3.22)

set(CMAKE_INSTALL_PREFIX "/usr")

set(CMAKE_CXX_STANDARD 23)

set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos")

SET(CMAKE_C_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
SET(CMAKE_ASM_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
SET(CMAKE_CXX_COMPILER_TARGET ${TOOLCHAIN_PREFIX})

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_ASM_COMPILER "clang")
set(CMAKE_AR "llvm-ar")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_SYSROOT ${SYSROOT})

set(COMMON_FLAGS
    -Wall -Wextra -O3 -g
    -fPIC
    -flto
    --sysroot=${SYSROOT}
    -ffat-lto-objects
)

project(pmoscxx
    VERSION 0.1
    DESCRIPTION "C++ helper library for pmOS"
    LANGUAGES CXX)

if (${TARGET_ARCH} STREQUAL "riscv64")
    list(APPEND COMMON_FLAGS -march=rva20u64 -mabi=lp64d)
endif()

add_compile_options(${COMMON_FLAGS})
add_link_options(${COMMON_FLAGS})

add_library(pmoscxx)

set(SRC_FOLDER "src")

file(GLOB_RECURSE SRC
    "${SRC_FOLDER}/*.cc"
)

target_sources(pmoscxx PRIVATE ${SRC})
target_include_directories(pmoscxx PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS pmoscxx)
install(DIRECTORY include/ DESTINATION include)