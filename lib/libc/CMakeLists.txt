cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 17)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(libc VERSION 0.1 LANGUAGES C ASM)

set(CMAKE_C_FLAGS "-Wall -Wextra -O2" CACHE STRING "C compiler flags")
set(CMAKE_ASM_FLAGS ${CMAKE_C_FLAGS} CACHE STRING "ASM compiler flags")

set(SRC_FOLDER "src")

file(GLOB_RECURSE GENERIC_SRC "${SRC_FOLDER}/generic/*.c" "${SRC_FOLDER}/generic/*.S")
file(GLOB_RECURSE ARCH_SRC "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.c" "${SRC_FOLDER}/arch/${TARGET_ARCH}/*.S")
set(SRC ${GENERIC_SRC} ${ARCH_SRC})

list(REMOVE_ITEM SRC "${SRC_FOLDER}/arch/${TARGET_ARCH}/crti.S" "${SRC_FOLDER}/arch/${TARGET_ARCH}/crtn.S")

# Main library
add_library(c STATIC ${SRC})
set_target_properties(c PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

# crti and crtn
add_library(crti_crtn OBJECT ${SRC_FOLDER}/arch/${TARGET_ARCH}/crti.S ${SRC_FOLDER}/arch/${TARGET_ARCH}/crtn.S)

set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../sysroot/usr/lib" CACHE PATH "Installation directory")
install(TARGETS c ARCHIVE DESTINATION ${INSTALL_DIR}/lib)

# https://stackoverflow.com/questions/75256690/how-to-install-object-files-using-cmake
macro(install_objects OBJFILE_LIST)
foreach(OBJFILE ${${OBJFILE_LIST}})
    # Available since CMake 3.20 you can use get_filename_component
    # To get any specific information you can wish for about a file
    # Before 3.20 you would have to use string(REGEX MATCH ...)
    # get_filename_component(OBJ_DIR ${OBJFILE} DIRECTORY) <-- path
    # get_filename_component(OBJ_EXT ${OBJFILE} EXT) <-- extension
   
    get_filename_component(OBJ_NAME ${OBJFILE} NAME_WE)
    
    # You can uncomment the message to see what you can get 
    # MESSAGE(STATUS "OBJ_DIR: ${OBJ_DIR} OBJ_NAME: ${OBJ_NAME} OBJ_EXT: ${OBJ_EXT}")

    # This is just an example, but you can also use ${OBJ_DIR} for example to specify a better path
    install(FILES ${OBJFILE} DESTINATION ${INSTALL_DIR}/lib RENAME "${OBJ_NAME}.o")
endforeach(OBJFILE)
endmacro(install_objects)

set(OBJECTS "${SRC_FOLDER}/arch/${TARGET_ARCH}/crti.S" "${SRC_FOLDER}/arch/${TARGET_ARCH}/crtn.S")
install_objects(OBJECTS)