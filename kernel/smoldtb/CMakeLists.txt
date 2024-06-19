cmake_minimum_required(VERSION 3.13.4)

set(SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(SRC "${SRCDIR}/smoldtb.c")

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

add_library(smoldtb STATIC)
target_include_directories(smoldtb PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(smoldtb PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_sources(smoldtb PRIVATE "${SRC}")