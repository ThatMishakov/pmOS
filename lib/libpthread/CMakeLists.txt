cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 17)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(libpthread VERSION 0.1 LANGUAGES C ASM)

set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -fPIC -g" CACHE STRING "C compiler flags")
set(CMAKE_ASM_FLAGS ${CMAKE_C_FLAGS} CACHE STRING "ASM compiler flags")

set(SRC_FOLDER "src")

file(GLOB_RECURSE GENERIC_SRC "${SRC_FOLDER}/generic/*.c" "${SRC_FOLDER}/generic/*.S")
file(GLOB_RECURSE ARCH_SRC "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.c" "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.S")
set(SRC ${GENERIC_SRC} ${ARCH_SRC})

add_library(pthread STATIC ${SRC})
set_target_properties(pthread PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sysroot/usr/lib" CACHE PATH "Installation directory")
install(TARGETS pthread ARCHIVE DESTINATION ${INSTALL_DIR})

# TODO: libc and libm might be dependencies