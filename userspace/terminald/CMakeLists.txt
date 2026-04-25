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

if(CMAKE_C_COMPILER_TARGET MATCHES "^i[3-6]86-pmos$")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lclang_rt.builtins-i386")
endif()

project(terminald C CXX)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

file(GLOB_RECURSE FLANTERM_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/flanterm/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/flanterm/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/flanterm/*.S")

set(FLANTERM_FB_DISABLE_BUMP_ALLOC 1)
set(FLANTERM_FB_SUPPORT_BPP 1)

add_executable(terminald ${GENERIC_SRC} ${FLANTERM_SRC})
set_property(TARGET terminald PROPERTY C_STANDARD 23)
set_property(TARGET terminald PROPERTY CXX_STANDARD 23)

target_include_directories(terminald PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/flanterm")

target_link_libraries(terminald pmoscxx)

install(TARGETS terminald RUNTIME DESTINATION "/boot")