cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 17)

set(TARGET_ARCH "x86_64" CACHE STRING "Target architecture (x86_64 or riscv64)")
set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos-")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_AR "${TOOLCHAIN_PREFIX}ar")

set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -fPIE -fshort-wchar" CACHE STRING "C compiler flags")
set(CMAKE_ASM_FLAGS ${CMAKE_C_FLAGS} CACHE STRING "ASM compiler flags")

project(loader VERSION 0.1 LANGUAGES C ASM)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/${TARGET_ARCH}.ld")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie -nostartfiles -lc -T ${LINKER_SCRIPT}")
set_target_properties(${TARGET_NAME} PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

set(SRC_FOLDER "src")

#file(GLOB_RECURSE ARCH_SRC "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.c" "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.S")
file(GLOB_RECURSE GENERIC_SRC "${SRC_FOLDER}/uefi.c")
set(SRC ${GENERIC_SRC} ${ARCH_SRC})
add_executable(loader ${SRC})
target_include_directories(loader PRIVATE "include")
