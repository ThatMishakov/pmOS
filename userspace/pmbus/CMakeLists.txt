cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -pipe")
set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -pipe")

project(pmbus CXX)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

add_executable(pmbus ${GENERIC_SRC})
set_property(TARGET pmbus PROPERTY C_STANDARD 23)
set_property(TARGET pmbus PROPERTY CXX_STANDARD 23)

target_link_libraries(pmbus pmoscxx)

install(TARGETS pmbus RUNTIME DESTINATION "/boot")