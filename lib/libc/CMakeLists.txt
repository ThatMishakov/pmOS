cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 17)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(CMAKE_ASM_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)


project(libc VERSION 0.1 LANGUAGES C ASM)

set(CMAKE_C_FLAGS "-Wall -Wextra -O2" CACHE STRING "C compiler flags")
set(CMAKE_ASM_FLAGS ${CMAKE_C_FLAGS} CACHE STRING "ASM compiler flags")

set(SRC_FOLDER "src")

file(GLOB_RECURSE GENERIC_SRC "${SRC_FOLDER}/generic/*.c" "${SRC_FOLDER}/generic/*.S")
file(GLOB_RECURSE ARCH_SRC "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.c" "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.S")
set(SRC ${GENERIC_SRC} ${ARCH_SRC})

list(REMOVE_ITEM SRC "${SRC_FOLDER}/arch/${TARGET_ARCH}/crti.S" "${SRC_FOLDER}/arch/${TARGET_ARCH}/crtn.S" "${SRC_FOLDER}/arch/${TARGET_ARCH}/crt0.S")

# Main library
add_library(c STATIC ${SRC})
set_target_properties(c PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

# crti and crtn
add_library(crtstuff OBJECT ${SRC_FOLDER}/arch/${TARGET_ARCH}/crti.S ${SRC_FOLDER}/arch/${TARGET_ARCH}/crtn.S ${SRC_FOLDER}/arch/${TARGET_ARCH}/crt0.S)

set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sysroot/usr/lib" CACHE PATH "Installation directory")
install(TARGETS c ARCHIVE DESTINATION ${INSTALL_DIR})

install(FILES $<TARGET_OBJECTS:crtstuff> DESTINATION ${INSTALL_DIR})