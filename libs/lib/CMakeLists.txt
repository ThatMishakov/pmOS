cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 17)

# ========================
# Toolchain configuration
# ========================

set(TOOLCHAIN_PREFIX "${TARGET_ARCH}-pmos")

set(CMAKE_C_COMPILER "clang")
set(CMAKE_ASM_COMPILER "clang")
set(CMAKE_AR "llvm-ar")

set(CMAKE_C_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
set(CMAKE_ASM_COMPILER_TARGET ${TOOLCHAIN_PREFIX})

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_ASM_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)

# ========================
# Global flags
# ========================

set(COMMON_FLAGS
    -Wall -Wextra -O3 -g
    -nostdlib
    -fPIC
    --sysroot=${SYSROOT}
    -fno-omit-frame-pointer
    -mno-omit-leaf-frame-pointer
    -flto
    -ffat-lto-objects
)

if (${TARGET_ARCH} STREQUAL "riscv64")
    list(APPEND COMMON_FLAGS -march=rva20u64 -mabi=lp64d)
endif()

add_compile_options(${COMMON_FLAGS})
add_link_options(${COMMON_FLAGS})

project(pmos_libs VERSION 0.1 LANGUAGES C ASM)

# ========================
# Subprojects
# ========================

add_subdirectory(libc)
add_subdirectory(libm)
add_subdirectory(librt)
add_subdirectory(libpthread)

# ========================
# Install (centralized)
# ========================

install(TARGETS c m rt pthread
        ARCHIVE DESTINATION "/usr/lib")

# libc installs crt files separately (see libc/CMakeLists.txt)