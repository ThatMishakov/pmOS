cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -pipe")
set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -pipe")

project(blockd CXX)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

add_executable(blockd ${GENERIC_SRC})
set_property(TARGET blockd PROPERTY C_STANDARD 23)
set_property(TARGET blockd PROPERTY CXX_STANDARD 23)

target_link_libraries(blockd pmoscxx)

install(TARGETS blockd RUNTIME DESTINATION "/boot")