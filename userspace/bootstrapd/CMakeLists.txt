cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED TARGET_ARCH)
  set(TARGET_ARCH "x86_64" CACHE STRING "Target architecture (x86_64, riscv64 or i686)")
endif()

# This is hacky...
set(BUILD_TESTING OFF CACHE BOOL "Disable tests for bootstrapd dependencies" FORCE)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -pipe")
set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -pipe")

project(bootstrapd C CXX ASM)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXE_LINKER_FLAGS "-static")

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

add_executable(bootstrapd ${GENERIC_SRC})

set_property(TARGET bootstrapd PROPERTY C_STANDARD 23)

include(FetchContent)
# Keep libyaml tests disabled even if the dependency declares its own option defaults.
set(YAML_BUILD_TESTS OFF CACHE BOOL "Disable libyaml tests" FORCE)
FetchContent_Declare(
  libyaml
  GIT_REPOSITORY https://github.com/yaml/libyaml.git
  GIT_TAG        0.2.5
)
FetchContent_MakeAvailable(libyaml)
target_link_libraries(bootstrapd PRIVATE yaml)
target_link_libraries(bootstrapd PRIVATE pmos-helpers)

install(TARGETS bootstrapd RUNTIME DESTINATION "/boot")