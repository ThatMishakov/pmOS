#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include "header.h"
#include <unistd.h>
#include "file.h"
#include <stdio.h>
#include <stdlib.h>

char *get_filename(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return NULL;
    }
    return argv[1];
}

int parse_archive(int fd, struct File ***file_pointer_array, size_t *file_count, size_t *file_capacity) {
    TARHeader header;
    ssize_t bytesRead;
    uint64_t next_header_offset = 0;
    off_t offset = 0;
    struct File *root = NULL;  // Global root directory

    while ((bytesRead = pread(fd, &header, sizeof(TARHeader), offset)) == sizeof(TARHeader)) {
        int result;
        struct File *file = malloc(sizeof(struct File));
        if (file == NULL) {
            perror("Failed to allocate memory for file");
            return -1;
        }
        file->refcount = 0;

        result = parse_header(&header, file, &next_header_offset, offset);
        if (result == 0) {
            // Successfully parsed the header

            // Check if the file_pointer_array capacity needs to be increased
            if (*file_count == *file_capacity) {
                // Increase the capacity by a fixed increment
                *file_capacity += 100;

                // Reallocate the file_pointer_array with the new capacity
                struct File **temp = realloc(*file_pointer_array, *file_capacity * sizeof(struct File *));
                if (temp == NULL) {
                    perror("Failed to reallocate memory for file_pointer_array");
                    free_file_buffers(file);  // Free the buffers of the allocated file struct
                    free(file);  // Free the allocated file struct
                    return -1;
                }
                *file_pointer_array = temp;
            }

            // Set the index of the file
            file->index = *file_count;

            // Add the parsed file to the file_pointer_array
            (*file_pointer_array)[*file_count] = file;
            (*file_count)++;

            // Increment the refcount of the file
            (*file_pointer_array)[file->index]->refcount++;

            // Resolve the parent directory
            struct File *parent = resolve_parent_dir(file);
            if (parent == NULL) {
                fprintf(stderr, "Failed to resolve parent directory for file: %s, path: %s\n", file->name, file->path == NULL? "NULL" : file->path);
                free_file_buffers(file);  // Free the buffers of the allocated file struct
                free(file);  // Free the allocated file struct
                return -1;
            }

            // Add the file to the parent directory's child array
            if (add_child_to_directory(parent, file) != 0) {
                fprintf(stderr, "Failed to add child to parent directory for file: %s\n", file->name);
                free_file_buffers(file);  // Free the buffers of the allocated file struct
                free(file);  // Free the allocated file struct
                return -1;
            }

            // Free the path as it is not needed anymore.
            free(file->path);
            file->path = NULL;
        } else if (result == -2) {
            // Reached the end of the archive
            return 0;
        } else {
            // Failed to parse the header
            fprintf(stderr, "Failed to parse the header.\n");
            free_file_buffers(file);  // Free the buffers of the allocated file struct
            free(file);  // Free the allocated file struct
            return -1;
        }

        offset += next_header_offset;
    }

    // Check if the end of the archive was not reached
    if (bytesRead != 0) {
        fprintf(stderr, "Archive ended unexpectedly.\n");
        return -1;
    }

    return 0;
}

void print_tree(const struct File *file, int indentation) {
    if (file == NULL) {
        return;
    }

    // Indent the file name based on the indentation level
    for (int i = 0; i < indentation; ++i) {
        printf("    "); // Assuming 4 spaces for each level of indentation
    }

    printf("%s\n", file->name);

    // Recursively print the child files/directories
    for (size_t i = 0; i < file->child_size; ++i) {
        struct File *child = file->child_array[i];
        print_tree(child, indentation + 1);
    }
}

int main(int argc, char *argv[]) {
    char *filename = get_filename(argc, argv);
    if (filename == NULL) {
        return 1;
    }

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open file");
        return 1;
    }

    struct File **file_pointer_array = NULL; // Array of pointers to File structs
    size_t file_count = 0;
    size_t file_capacity = 0;

    int result = parse_archive(fd, &file_pointer_array, &file_count, &file_capacity);

    if (result == 0) {
        // Parsing successful
        for (size_t i = 0; i < file_count; i++) {
            struct File *file = file_pointer_array[i];
            printf("File Name: %s\n", file->name);
            if (file->path != NULL) {
                printf("File Path: %s\n", file->path);
            }
            printf("File Size: %zu\n", file->file_size);
            printf("Last Modified Time: %lu\n", file->last_modified_time);
            printf("Type: %u\n", file->type);
            printf("File Mode: %u\n", file->file_mode);
            printf("Owner UID: %u\n", file->owner_uid);
            printf("Owner GID: %u\n", file->owner_gid);
            printf("Device Major: %u\n", file->device_major);
            printf("Device Minor: %u\n", file->device_minor);
            printf("Header Offset: %zu\n", file->header_offset);
            printf("RefCount: %u\n", file->refcount); // Print refcount field
            // ... Print other file details ...
            printf("\n");
        }

        print_tree(&root_directory, 0);
    } else {
        // Parsing failed
        fprintf(stderr, "Failed to parse the archive.\n");
    }

    // Release the file resources
    for (size_t i = 0; i < file_count; i++) {
        release_file(file_pointer_array[i]);
    }
    free(file_pointer_array);
    close(fd);

    // Clean up the dummy root buffers
    cleanup_root_buffers();

    return result;
}

