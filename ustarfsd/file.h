#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include "header.h"

/**
 * @brief File descriptor
 * 
 * This structure describes a file within the working tar archive
 * 
 */
struct File {
    size_t file_size;
    off_t header_offset;

    uint64_t last_modified_time;

    uint32_t type;

    uint32_t file_mode;
    uint32_t owner_uid;
    uint32_t owner_gid;
    
    uint32_t device_major;
    uint32_t device_minor;

    size_t name_length;
    char * name;

    int type;
};

/**
 * @brief Allocates the name buffer
 * 
 * @param file File descriptor where to allocate the name
 * @param length Length of the name
 * @return int 0 on success, -1 otherwise
 */
int allocate_name(struct file * file, int length);

/**
 * @brief Deallocates the buffers inside the file
 * 
 * @param file File whose structures need deallocation
 */
void deallocate_file(struct file * file);

/**
 * @brief Parses the TAR header into the given file
 * 
 * @param header Header to be parsed
 * @param file File where to save the parsed info
 * @param next_header_offset If not null, the offset from the current header to the next header
 * @return int 0 on success, -1 otherwise
 */
int parse_header(const struct * TARHeader header, struct * File file, uint64_t * next_header_offset);

#endif