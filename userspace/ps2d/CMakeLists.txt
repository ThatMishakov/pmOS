cmake_minimum_required(VERSION 3.22)

project(ps2d C)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

add_executable(ps2d ${GENERIC_SRC})
set_property(TARGET ps2d PROPERTY C_STANDARD 23)
set_property(TARGET ps2d PROPERTY CXX_STANDARD 23)

target_link_libraries(ps2d PRIVATE pmos-helpers)

install(TARGETS ps2d RUNTIME DESTINATION "/boot")