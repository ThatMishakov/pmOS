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
 * @struct File
 * @brief Represents a file entry in the archive.
 *
 * The `File` struct represents a file entry in the archive. It contains information
 * about the file, such as its name, size, type, permissions, ownership, and other
 * metadata. It is used to construct a hierarchical representation of the files
 * in the archive.
 */
struct File {
    char *name;                 /**< File name */
    char *path;                 /**< Pointer to the path component of the filename */
    off_t header_offset;        /**< Offset of the header within the archive */
    size_t file_size;           /**< Size of the file in bytes */
    time_t last_modified_time;  /**< Last modified time of the file */
    FileType type;              /**< Type of the file (regular file, directory, etc.) */
    mode_t file_mode;           /**< File mode/permissions */
    uid_t owner_uid;            /**< Owner user ID */
    gid_t owner_gid;            /**< Owner group ID */
    dev_t device_major;         /**< Device major number (if applicable) */
    dev_t device_minor;         /**< Device minor number (if applicable) */
    uint32_t refcount;          /**< Reference count for the file */
    struct File *parent;        /**< Pointer to the parent directory */
    struct File **child_array;  /**< Array of child files (for directories) */
    size_t child_size;          /**< Size of the child array (for directories) */
    size_t child_capacity;      /**< Capacity of the child array (for directories) */
    size_t index;               /**< Index of the file in the file_array */
};


/**
 * @brief Global variable representing the root directory.
 *
 * This variable serves as the root of the file hierarchy tree. It is a dummy
 * struct representing the root directory itself, and it holds the array of
 * files in the root directory.
 */
extern struct File root_directory;

/**
 * @brief Cleans up the buffers and child files associated with the root directory.
 *
 * This function releases the dynamically allocated buffers and child files associated with the root directory.
 * It recursively releases all the child files in the child array of the root directory using the `release_file` function.
 * After releasing the child files, it frees the child array itself and resets the child-related fields of the root directory.
 *
 * @note This function assumes that the root directory is represented by the global variable `root_directory`.
 *       If there are other global variables or resources related to the root directory, they should be handled accordingly.
 */
void cleanup_root_buffers();


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
 * @brief Parses the TAR header and populates a File structure.
 *
 * This function parses the TAR header provided and populates the corresponding fields in the File structure.
 * It performs header checksum verification, copies file size, last modified time, file type, file mode, owner UID, owner GID,
 * device major and minor numbers, and resolves and splits the filename into path and name components.
 *
 * @param header The TAR header to parse.
 * @param file Pointer to the File structure to populate.
 * @param next_header_offset Pointer to store the offset to the next header.
 * @param absolute_header_offset The absolute offset of the current header within the TAR archive.
 * @return 0 on success, -1 if header checksum verification fails, -2 if it's an end-of-archive header, -3 if an unexpected file type is encountered,
 * -4 if memory allocation fails for the name, -5 if resolving and cleaning up the filename fails, -6 if splitting the filename fails.
 */
int parse_header(const TARHeader* header, struct File* file, uint64_t* next_header_offset, uint64_t absolute_header_offset);

/**
 * @brief Macro for the increment value of the child array in a directory.
 */
#define CHILD_ARRAY_INCREMENT 100


/**
 * @brief Adds a child file entry to a parent directory.
 *
 * This function adds a child file entry to a parent directory by updating the parent's child_array.
 * The child file entry is appended to the end of the child_array.
 * The `refcount` of the child file is incremented.
 *
 * @param parent The parent directory file entry.
 * @param child The child file entry to be added.
 * @return 0 if the child is successfully added, -1 otherwise.
 *
 * @note If the parent is NULL, an error message is printed and -1 is returned.
 * @note If the child is NULL, -1 is returned.
 * @note If the parent's child_array capacity needs to be increased and the reallocation fails, -1 is returned.
 */
int add_child_to_directory(struct File *parent, struct File *child);

/**
 * Resolves the parent directory for a given file in the file hierarchy.
 *
 * @param file The file for which to resolve the parent directory.
 * @return The parent directory of the file, or NULL if the parent was not found.
 */
struct File *resolve_parent_dir(const struct File *file);

/**
 * @brief Splits a filename into path and name components.
 *
 * This function takes a filename as input and splits it into path and name components.
 * The path and name buffers are allocated dynamically and must be freed by the caller.
 *
 * @param filename The filename to split.
 * @param[out] path Pointer to store the path component.
 * @param[out] name Pointer to store the name component.
 * @return 0 on success, or an error code if an error occurs.
 */
int split_filename_path(const char *filename, char **path, char **name);

/**
 * @brief Resolves and cleans up a filename by removing single dot and double dot directories.
 *
 * This function takes a filename as input and resolves and cleans it up by removing single dot and double dot directories.
 * The filename is modified in-place.
 *
 * @param filename The filename to resolve and clean up.
 * @return 0 on success, or an error code if an error occurs.
 */
int resolve_filename(char *filename);



#endif