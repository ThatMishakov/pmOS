cmake_minimum_required(VERSION 3.22)

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

project(devicesd ASM C)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.S")

file(GLOB_RECURSE PLATFORM_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/arch/${TARGET_ARCH}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/arch/${TARGET_ARCH}/*.S")

add_executable(devicesd ${GENERIC_SRC} ${PLATFORM_SRC})
set_property(TARGET devicesd PROPERTY C_STANDARD 23)
set_property(TARGET devicesd PROPERTY CXX_STANDARD 23)

target_include_directories(devicesd PRIVATE "include")

include(${CMAKE_SOURCE_DIR}/uACPI/uacpi.cmake)

target_sources(devicesd PRIVATE
  ${UACPI_SOURCES}
)

target_include_directories(devicesd PRIVATE
  ${UACPI_INCLUDES}
)

install(TARGETS devicesd RUNTIME DESTINATION "/boot")