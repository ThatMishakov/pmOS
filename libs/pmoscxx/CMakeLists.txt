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

add_library(objlib OBJECT)

set(SRC_FOLDER "src")

file(GLOB_RECURSE SRC
    "${SRC_FOLDER}/*.cc"
)

target_sources(objlib PRIVATE ${SRC})
target_include_directories(objlib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_property(TARGET objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(pmoscxx_static STATIC $<TARGET_OBJECTS:objlib>)
add_library(pmoscxx_shared SHARED $<TARGET_OBJECTS:objlib>)

set_target_properties(pmoscxx_static PROPERTIES OUTPUT_NAME "pmoscxx")
set_target_properties(pmoscxx_shared PROPERTIES OUTPUT_NAME "pmoscxx")

install(TARGETS pmoscxx_static pmoscxx_shared)
install(DIRECTORY include/ DESTINATION include)