cmake_minimum_required(VERSION 3.22)

# This is hacky...
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

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

project(kernel C CXX ASM)

get_filename_component(ABSPATH "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(SYSROOT "${ABSPATH}/../sysroot")

include(arch/arch.cmake)
LOAD_PLATFORM(${TARGET_ARCH})

set(CMAKE_C_FLAGS "${PLATFORM_C_FLAGS} -static -ffreestanding -fstack-protector-all -flto -Og -Wall -Wextra -g -fno-omit-frame-pointer -fasynchronous-unwind-tables  -fexceptions --sysroot=${SYSROOT}")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_CXX_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_ASM_FLAGS}")
add_compile_options("-fcheck-new")

set(LIBUNWIND_ENABLE_SHARED OFF)
set(LIBUNWIND_IS_BAREMETAL ON)
set(LIBUNWIND_INSTALL_HEADERS OFF)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.S")

add_executable(kernel ${PLATFORM_SRCS} ${GENERIC_SRC})

set_target_properties(kernel PROPERTIES CXX_STANDARD 20)
set_target_properties(kernel PROPERTIES LINK_FLAGS 
  "-nostdlib -ffreestanding -static -T${CMAKE_CURRENT_SOURCE_DIR}/arch/${TARGET_ARCH}/linker.ld -z max-page-size=0x1000 -Wl,--eh-frame-hdr -Wl,--demangle")
if (${TARGET_ARCH} STREQUAL "i686")
  set_property(TARGET kernel APPEND_STRING PROPERTY LINK_FLAGS " -lclang_rt.builtins-i386")
else()
  add_compile_options("-fPIE")
  set_property(TARGET kernel APPEND_STRING PROPERTY LINK_FLAGS " -pie -Wl,-pie")
endif()

target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/arch/${TARGET_ARCH}")

if(${TARGET_ARCH} STREQUAL "x86_64" OR ${TARGET_ARCH} STREQUAL "i686")
  target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/arch/x86")
endif()

target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/generic")

add_subdirectory(smoldtb)

if (DEFINED ENABLE_DTB)
  target_link_libraries(kernel PUBLIC smoldtb)
  add_compile_definitions(ENABLE_DTB)
endif()
