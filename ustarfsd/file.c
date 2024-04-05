/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "file.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct File root_directory = {
    .name = "/",
    .file_size = 0,
    .last_modified_time = 0,
    .type = TYPE_DIRECTORY,
    .file_mode = 0755,
    .owner_uid = 0,
    .owner_gid = 0,
    .device_major = 0,
    .device_minor = 0,
    .header_offset = 0,
    .child_size = 0,
    .child_array = NULL,
    .child_capacity = 0,
    .refcount = 0,
    .index = 0
};


static bool is_extended_size(const int8_t *size_field) {
    // Check if the size field contains the extended size indicator
    for (size_t i = 0; i < 12; ++i) {
        if (size_field[i] != 0xFF)
            return false;
    }
    return true;
}

static uint64_t parse_extended_size(const int8_t *size_field) {
    uint64_t size = 0;
    for (size_t i = 0; i < 12; ++i) {
        size <<= 8;
        size += (uint8_t)size_field[i];
    }
    return size;
}

int parse_header(const TARHeader *header, struct File *file, uint64_t *next_header_offset, uint64_t absolute_header_offset) {
    // Calculate the checksum of the header
    int header_checksum = calculate_checksum(header);

    // Convert the checksum field to an integer
    int header_checksum_field = strtol(header->checksum, NULL, 8);

    // Verify the checksum
    if (header_checksum != header_checksum_field) {
        if (is_empty_header(header)) {
            *next_header_offset = 0;
            return -2;  // End of archive
        }
        fprintf(stderr, "Header checksum verification failed.\n");

        char * read_str = strndup((const char *)header, 512);
        fprintf(stderr, "Read: %s\n", read_str);
        free(read_str);
        return -1;
    }

    // Copy file size
    const int8_t *size_field = header->file_size;
    if (is_extended_size(size_field)) {
        file->file_size = parse_extended_size(size_field);
    } else {
        file->file_size = strtol(size_field, NULL, 8);
    }

    // Copy last modified time
    file->last_modified_time = strtol(header->last_mod_time, NULL, 8);

    // Convert file type to enum
    int type = header->type - '0';
    if (type < TYPE_REGULAR || type > TYPE_CONTIGUOUS_FILE) {
        fprintf(stderr, "Unexpected file type: %c\n", header->type);
        return -3;
    }
    file->type = (FileType)type;

    // Copy file mode, owner UID, and owner GID
    file->file_mode = strtol(header->file_mode, NULL, 8);
    file->owner_uid = strtol(header->owner_id, NULL, 8);
    file->owner_gid = strtol(header->group_id, NULL, 8);

    // Copy device major and minor numbers
    file->device_major = strtol(header->device_major_number, NULL, 8);
    file->device_minor = strtol(header->device_minor_number, NULL, 8);

    // Calculate the length of the filename, accounting for the null terminator
    size_t filename_length = strnlen(header->file_name, sizeof(header->file_name));
    size_t prefix_length = strnlen(header->filename_prefix, sizeof(header->filename_prefix));
    size_t name_length = filename_length + prefix_length;

    // Allocate memory for the name
    char *name = malloc(name_length + 1);
    if (name == NULL) {
        fprintf(stderr, "Failed to allocate memory for the name.\n");
        return -4;
    }

    // Concatenate the filename and prefix
    strncpy(name, header->filename_prefix, prefix_length);
    strncpy(name + prefix_length, header->file_name, filename_length);
    name[name_length] = '\0';

    // Resolve and clean up filename
    if (resolve_filename(name) != 0) {
        fprintf(stderr, "Failed to resolve and clean up filename: %s\n", name);
        free(name);
        return -5;
    }

    // Split the filename into path and name components
    char *path = NULL;
    if (split_filename_path(name, &path, &file->name) != 0) {
        fprintf(stderr, "Failed to split filename into path and name: %s\n", name);
        free(name);
        return -6;
    }

    // Set the path and name pointers in the File struct
    file->path = path;

    // Free the previously allocated name buffer
    free(name);

    // Initialize child size, child array, and child capacity
    file->child_size = 0;
    file->child_array = NULL;
    file->child_capacity = 0;

    // Calculate the size of the header (including padding)
    size_t header_size = sizeof(TARHeader);
    size_t padding = (512 - (header_size % 512)) % 512;

    // Calculate the size of the file content (including padding)
    size_t file_content_size = file->file_size;
    size_t file_content_padding = (512 - (file_content_size % 512)) % 512;

    // Calculate the offset to the next header (accounting for padding)
    *next_header_offset = header_size + padding + file_content_size + file_content_padding;

    // Set the header offset within the file
    file->header_offset = absolute_header_offset;

    return 0;
}

void free_file_buffers(struct File *file) {
    if (file == NULL) {
        return;
    }

    // Free the name buffer
    free(file->name);

    //Free the path buffer
    free(file->path);

    // Free the child array and its contents
    if (file->child_array != NULL) {
        for (size_t i = 0; i < file->child_size; i++) {
            release_file(file->child_array[i]);
        }
        free(file->child_array);
    }

    // Reset the child-related fields
    file->child_size = 0;
    file->child_capacity = 0;
    file->child_array = NULL;

    // Reset the parent and refcount
    file->parent = NULL;
    file->refcount = 0;
}

void release_file(struct File *file) {
    if (file == NULL) {
        return;
    }

    if (file->refcount > 0) {
        file->refcount--;
        if (file->refcount == 0) {
            free_file_buffers(file);
            free(file);
        }
    }
}

size_t binarySearchUpperBound(struct File **array, size_t low, size_t high, const char *name) {
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (strcmp(name, array[mid]->name) >= 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

int add_child_to_directory(struct File *parent, struct File *child) {
    if (parent == NULL) {
        fprintf(stderr, "Error: Parent directory is NULL.\n");
        return -1;
    }

    if (child == NULL) {
        fprintf(stderr, "Error: Child file is NULL.\n");
        return -1;
    }

    // Increase the parent directory's child_array capacity if needed
    if (parent->child_size == parent->child_capacity) {
        parent->child_capacity += CHILD_ARRAY_INCREMENT;

        struct File **temp = realloc(parent->child_array, parent->child_capacity * sizeof(struct File *));
        if (temp == NULL) {
            perror("Failed to reallocate memory for child_array");
            return -1;
        }
        parent->child_array = temp;
    }

    // Find the upper bound index where the child should be inserted based on name using binary search
    size_t insertIndex = binarySearchUpperBound(parent->child_array, 0, parent->child_size, child->name);

    // Shift existing elements to the right to make space for the new child
    memmove(parent->child_array + insertIndex + 1,
            parent->child_array + insertIndex,
            (parent->child_size - insertIndex) * sizeof(struct File *));

    // Insert the child file at the correct index
    parent->child_array[insertIndex] = child;
    parent->child_size++;

    // Increment the refcount of the child file
    child->refcount++;

    return 0;
}

struct File *resolve_parent_dir(const struct File *file) {
    if (file == NULL) {
        return NULL;
    }

    const char *filepath = file->path;
    struct File *currentDir = &root_directory;

    // Check if the filepath is NULL (file in the root directory)
    if (filepath == NULL) {
        return currentDir;
    }

    while (currentDir != NULL && filepath != NULL) {
        // Step over consecutive '/' characters in the filepath
        while (*filepath == '/') {
            ++filepath;
        }

        // Reached the end of the file
        if (*filepath == '\0') {
            return currentDir;
        }

        // Find the first '/' character in the filepath
        const char *nextSlash = strchr(filepath, '/');

        if (nextSlash == NULL) {
            // No '/' found, the remaining filepath is the file name
            const char *fileName = filepath;

            // Binary search for the file in the sorted child_array of the current directory
            size_t left = 0;
            size_t right = currentDir->child_size;
            while (left < right) {
                size_t mid = left + (right - left) / 2;
                struct File *child = currentDir->child_array[mid];

                int comparison = strcmp(child->name, fileName);
                if (comparison == 0) {
                    // File found, return the parent directory
                    return currentDir;
                } else if (comparison < 0) {
                    left = mid + 1;
                } else {
                    right = mid;
                }
            }

            // File not found, return NULL
            return NULL;
        }

        // Compute the length of the directory name
        size_t dirNameLength = nextSlash - filepath;

        // Binary search for the directory in the sorted child_array of the current directory
        size_t left = 0;
        size_t right = currentDir->child_size;
        struct File *nextDir = NULL;
        while (left < right) {
            size_t mid = left + (right - left) / 2;
            struct File *child = currentDir->child_array[mid];

            int comparison = strncmp(child->name, filepath, dirNameLength);
            if (comparison == 0 && child->name[dirNameLength] == '\0') {
                // Directory found, update nextDir
                nextDir = child;
                break;
            } else if (comparison < 0 || (comparison == 0 && child->name[dirNameLength] < '\0')) {
                left = mid + 1;
            } else {
                right = mid;
            }
        }

        // If nextDir is not found, return NULL
        if (nextDir == NULL) {
            return NULL;
        }

        // Update currentDir and filepath for the next iteration
        currentDir = nextDir;
        filepath = nextSlash + 1;
    }

    // Parent directory not found
    return NULL;
}

void cleanup_root_buffers() {
    if (root_directory.child_array != NULL) {
        for (size_t i = 0; i < root_directory.child_size; i++) {
            release_file(root_directory.child_array[i]);
        }
        free(root_directory.child_array);
    }

    // Reset the child-related fields
    root_directory.child_size = 0;
    root_directory.child_capacity = 0;
    root_directory.child_array = NULL;
}

int resolve_filename(char *filename) {
    // Check if the filename is empty
    if (filename == NULL || filename[0] == '\0') {
        return -1;
    }

    char *resolved = filename;
    char *current = resolved;
    char *previous = resolved;

    while (*current != '\0') {
        if (*current == '.' && *(current + 1) == '.') {
            // Found double dots, remove previous directory
            if (previous == resolved) {
                // Cannot remove previous directory, invalid filename
                return -1;
            }
            while (*previous != '/') {
                previous--;
            }
            if (previous == resolved && *(previous + 1) == '\0') {
                // Cannot remove previous directory, invalid filename
                return -1;
            }
            current += 2;
        } else if (*current == '.' && *(current + 1) == '/') {
            // Found single dot, remove dot and slash
            current += 2;
        } else {
            // Copy character to resolved name
            *resolved = *current;
            resolved++;
            current++;
        }
    }
    *resolved = '\0';

    // Remove preceding slashes
    size_t start = 0;
    while (filename[start] == '/') {
        start++;
    }

    // Remove trailing slashes
    size_t end = strlen(filename) - 1;
    while (end > start && filename[end] == '/') {
        end--;
    }

    // Shift the filename to remove preceding slashes
    if (start > 0) {
        memmove(filename, filename + start, end - start + 2);
        end -= start;
    }

    // Remove trailing slashes by adjusting the null termination
    filename[end + 1] = '\0';

    return 0;
}

int split_filename_path(const char *filename, char **path, char **name) {
    size_t len = strlen(filename);
    *path = NULL;
    *name = NULL;

    // Find the last occurrence of '/' in the filename
    const char *last_slash = strrchr(filename, '/');

    if (last_slash == NULL) {
        // No path component, only a name
        *name = strdup(filename);
        if (*name == NULL) {
            // Memory allocation failed
            return -1;
        }
    } else {
        // Path component exists
        size_t path_length = last_slash - filename + 1;
        *path = malloc(path_length + 1);
        if (*path == NULL) {
            // Memory allocation failed
            return -1;
        }
        strncpy(*path, filename, path_length);
        (*path)[path_length] = '\0';

        size_t name_length = len - path_length;
        *name = malloc(name_length + 1);
        if (*name == NULL) {
            // Memory allocation failed
            free(*path);
            *path = NULL;
            return -1;
        }
        strncpy(*name, last_slash + 1, name_length);
        (*name)[name_length] = '\0';
    }

    return 0;
}