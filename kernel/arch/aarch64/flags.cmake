set(PLATFORM_C_FLAGS "-mgeneral-regs-only")
set(PLATFORM_ASM_FLAGS "-mgeneral-regs-only")

set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/user_access.S"
    PROPERTIES COMPILE_FLAGS "-march=armv8.1-a"
)