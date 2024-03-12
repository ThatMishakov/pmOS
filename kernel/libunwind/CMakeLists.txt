cmake_minimum_required(VERSION 3.13.4)

add_compile_definitions(_LIBUNWIND_IS_BAREMETAL)

set(SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(SRC "${SRCDIR}/libunwind.cpp"
                "${SRCDIR}/Unwind-seh.cpp"
                "${SRCDIR}/Unwind-sjlj.c"
                "${SRCDIR}/UnwindLevel1-gcc-ext.c"
                "${SRCDIR}/UnwindLevel1.c"
                "${SRCDIR}/UnwindRegistersRestore.S"
                "${SRCDIR}/UnwindRegistersSave.S")

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

add_library(libunwind-kernel STATIC)
target_include_directories(libunwind-kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(libunwind-kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_sources(libunwind-kernel PRIVATE "${SRC}")