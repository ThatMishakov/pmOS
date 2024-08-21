cmake_minimum_required(VERSION 3.22)

# This is hacky...
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

if(NOT DEFINED TARGET_ARCH)
  set(TARGET_ARCH "x86_64" CACHE STRING "Target architecture (x86_64 or riscv64)")
endif()

set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos")

SET(CMAKE_C_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
SET(CMAKE_ASM_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
SET(CMAKE_CXX_COMPILER_TARGET ${TOOLCHAIN_PREFIX})

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_ASM_COMPILER "clang")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}-ar")

project(kernel C CXX ASM)

get_filename_component(ABSPATH "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(SYSROOT "${ABSPATH}/../sysroot")

include(arch/arch.cmake)
LOAD_PLATFORM(${TARGET_ARCH})

set(CMAKE_C_FLAGS "${PLATFORM_C_FLAGS} -Og -static -nostdlib -fPIE -Wall -Wextra -g -fno-omit-frame-pointer -fasynchronous-unwind-tables  -fexceptions --sysroot=${SYSROOT}")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_CXX_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_ASM_FLAGS}")

set(LIBUNWIND_ENABLE_SHARED OFF)
set(LIBUNWIND_IS_BAREMETAL ON)
set(LIBUNWIND_INSTALL_HEADERS OFF)

add_subdirectory(libunwind)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.S")

add_executable(kernel ${PLATFORM_SRCS} ${GENERIC_SRC})

set_target_properties(kernel PROPERTIES CXX_STANDARD 20)
set_target_properties(kernel PROPERTIES LINK_FLAGS 
  "-nostdlib -ffreestanding -static -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -pie -z max-page-size=0x1000 -Wl,--eh-frame-hdr -Wl,--demangle -Wl,-pie")

target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/arch/${TARGET_ARCH}")
target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/generic")

set(CXXRT_SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/libcxxrt/src")
set(CXXRT_FILES
    "${CXXRT_SRCDIR}/cxa_atexit.c"
    "${CXXRT_SRCDIR}/cxa_finalize.c"
    "${CXXRT_SRCDIR}/dynamic_cast.cc"
    "${CXXRT_SRCDIR}/guard.cc"
    "${CXXRT_SRCDIR}/memory.cc"
    "${CXXRT_SRCDIR}/stdexcept.cc"
    "${CXXRT_SRCDIR}/typeinfo.cc"
    "${CXXRT_SRCDIR}/terminate.cc"
    "${CXXRT_SRCDIR}/auxhelper.cc"
    "${CXXRT_SRCDIR}/exception.cc")

add_library(cxxrt STATIC)
target_sources(cxxrt PRIVATE ${CXXRT_FILES})
target_include_directories(cxxrt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(kernel PUBLIC cxxrt)
target_link_libraries(kernel PUBLIC libunwind-kernel)

add_subdirectory(smoldtb)

if (DEFINED ENABLE_DTB)
  target_link_libraries(kernel PUBLIC smoldtb)
  add_compile_definitions(ENABLE_DTB)
endif()
