cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -pipe")
set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -pipe")

project(i8042 C)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

add_executable(i8042 ${GENERIC_SRC})
set_property(TARGET i8042 PROPERTY C_STANDARD 23)

target_link_libraries(i8042 PRIVATE pmos-helpers)

install(TARGETS i8042 RUNTIME DESTINATION "/boot")