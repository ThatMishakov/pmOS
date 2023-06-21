#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include "header.h"
#include <sys/types.h>

typedef enum {
    TYPE_REGULAR = 0,
    TYPE_HARD_LINK,
    TYPE_SYMBOLIC_LINK,
    TYPE_CHARACTER_SPECIAL,
    TYPE_BLOCK_SPECIAL,
    TYPE_DIRECTORY,
    TYPE_FIFO,
    TYPE_CONTIGUOUS_FILE
} FileType;

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

    FileType type;

    uint32_t file_mode;
    uint32_t owner_uid;
    uint32_t owner_gid;
    
    uint32_t device_major;
    uint32_t device_minor;

    size_t name_length;
    char * name;

    size_t child_size;
    struct File * child_array;

    uint64_t refcount;
    uint64_t index;
};

/**
 * @brief Allocates the name buffer
 * 
 * @param file File descriptor where to allocate the name
 * @param length Length of the name
 * @return int 0 on success, -1 otherwise
 */
int allocate_name(struct File * file, int length);

/**
 * @brief Frees the allocated buffers of a File structure.
 *
 * This function frees the dynamically allocated buffers (name and child_array) of the specified File structure.
 * It should be called when the File structure is no longer needed to release the allocated memory.
 * If the File structure has child files in the child_array, this function recursively releases them as well.
 *
 * @param file Pointer to the File structure.
 */
void free_file_buffers(struct File *file);

/**
 * @brief Releases a File structure and its associated resources.
 *
 * This function releases a File structure and its associated resources. It decrements the reference count of the File structure.
 * If the reference count reaches zero, it frees the dynamically allocated buffers (name and child_array) and frees the File structure itself.
 * If the File structure has child files in the child_array, this function recursively releases them as well.
 *
 * @param file Pointer to the File structure.
 */
void release_file(struct File *file);


/**
 * @brief Parses the TAR header and populates the File struct with the extracted information.
 *
 * This function takes a TARHeader struct, extracts the relevant information, and stores it in
 * a File struct. It also calculates the offset to the next header and sets the header offset
 * within the file. The function performs checksum verification on the header to ensure its integrity.
 *
 * @param header The TARHeader struct to be parsed.
 * @param file Pointer to the File struct where the extracted information will be stored.
 * @param next_header_offset Pointer to a variable that will hold the offset to the next header.
 * @param absolute_header_offset The absolute offset of the current header within the file.
 * @return 0 if the header is successfully parsed and verified, -1 if the checksum verification fails,
 *         -2 if the end of the archive is encountered (empty header), -3 if an invalid header type is encountered.
 */
int parse_header(const TARHeader* header, struct File* file, uint64_t* next_header_offset, uint64_t absolute_header_offset);

#endif