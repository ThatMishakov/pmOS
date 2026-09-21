cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED TARGET_ARCH)
  set(TARGET_ARCH "x86_64" CACHE STRING "Target architecture (x86_64, riscv64 or i686)")
endif()

project(kernel C CXX ASM)

get_filename_component(ABSPATH "${CMAKE_CURRENT_LIST_FILE}" PATH)

include(arch/arch.cmake)
LOAD_PLATFORM(${TARGET_ARCH})

set(CMAKE_C_FLAGS "${PLATFORM_C_FLAGS} -static -ffreestanding -fstack-protector-all -O3 -Wall -Wextra -ggdb3 -fno-omit-frame-pointer -fasynchronous-unwind-tables -fexceptions")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_CXX_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} ${PLATFORM_ASM_FLAGS}")
add_compile_options("-fcheck-new")

file(GLOB_RECURSE GENERIC_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/generic/*.S")

add_executable(kernel ${PLATFORM_SRCS} ${GENERIC_SRC})

set_target_properties(kernel PROPERTIES CXX_STANDARD 26)
set_target_properties(kernel PROPERTIES LINK_FLAGS 
  "-nostdlib -ffreestanding -static -T${CMAKE_CURRENT_SOURCE_DIR}/arch/${TARGET_ARCH}/linker.ld -z max-page-size=0x1000 -Wl,--eh-frame-hdr -Wl,--demangle")
if (${TARGET_ARCH} STREQUAL "i686")
  set_property(TARGET kernel APPEND_STRING PROPERTY LINK_FLAGS " -static")
  target_link_libraries(kernel PUBLIC atomic gcc)
elseif (${TARGET_ARCH} STREQUAL "m68k")
  target_link_libraries(kernel PUBLIC gcc)
elseif (NOT ${TARGET_ARCH} STREQUAL "x86_64")
  # NB: target_compile_options (not add_compile_options): the kernel target
  # already exists at this point, and directory options would not apply to it.
  target_compile_options(kernel PRIVATE "-fPIE")
  set_property(TARGET kernel APPEND_STRING PROPERTY LINK_FLAGS " -pie -Wl,-pie")
else()
  target_compile_options(kernel PRIVATE "-mcmodel=kernel")
endif()

target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/arch/${TARGET_ARCH}")

if(${TARGET_ARCH} STREQUAL "x86_64" OR ${TARGET_ARCH} STREQUAL "i686")
  target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/arch/x86")
endif()

target_include_directories(kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/generic")

add_subdirectory(smoldtb)

# smoldtb is statically linked into the kernel, so it needs the same code
# model as the kernel target itself (it is a separate CMake target and does
# not inherit the options above).
if (${TARGET_ARCH} STREQUAL "x86_64")
  target_compile_options(smoldtb PRIVATE "-mcmodel=kernel")
elseif (NOT ${TARGET_ARCH} STREQUAL "i686")
  target_compile_options(smoldtb PRIVATE "-fPIE")
endif()

if (DEFINED ENABLE_DTB)
  target_link_libraries(kernel PUBLIC smoldtb)
  add_compile_definitions(ENABLE_DTB)
endif()

if (DEFINED ENABLE_ACPI)
  target_compile_definitions(kernel PRIVATE
    ENABLE_ACPI
  )

  include(${CMAKE_SOURCE_DIR}/uACPI/uacpi.cmake)

  target_sources(kernel PRIVATE
    ${UACPI_SOURCES}
  )

  target_include_directories(kernel PRIVATE
    ${UACPI_INCLUDES}
  )

  target_compile_definitions(kernel PRIVATE
    UACPI_BAREBONES_MODE
  )
endif()

install(TARGETS kernel RUNTIME DESTINATION "/boot")