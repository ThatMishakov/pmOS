#ifndef __PMOS_FILE_H
#define __PMOS_FILE_H
#include "ipc.h"
#include "system.h"

/// pmOS memory object identificator
typedef unsigned long mem_object_t;

struct pmOS_File {
    pmos_port_t file_handler;
    mem_object_t memory_object;
};

/**
 * @brief Opens a file
 * 
 * This function sends a request to the VFS server and opens a file descriptor. The file is an abstract type which, among other things,
 * might point to a file, folder, block device or others. '\' is used as a file deliminer.
 * 
 * @param desc File descriptor to be filled. If the execution is not successfull, it shall not change.
 * @param name Name of the file. This string can contain any characters and '\0' is not considered to be a string terminator.
 * @param length Length of the string.
 * @param flags Flags for opening the file.
 * @return Result of the execution. SUCCESS macro indicates that the operation was successfull. Otherwise, the returned number indicates the error number.
 */
result_t pmos_open_file(pmOS_File *desc, const unsigned char * name, size_t length, uint64_t flags);

#endif