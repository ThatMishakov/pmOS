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

int parse_archive(int fd, struct File **file_array, size_t *file_count, size_t *file_capacity) {
    TARHeader header;
    ssize_t bytesRead;
    uint64_t next_header_offset = 0;
    off_t offset = 0;

    while ((bytesRead = pread(fd, &header, sizeof(TARHeader), offset)) == sizeof(TARHeader)) {
        int result;
        struct File *file = malloc(sizeof(struct File));
        if (file == NULL) {
            perror("Failed to allocate memory for file");
            return -1;
        }

        result = parse_header(&header, file, &next_header_offset, offset);
        if (result == 0) {
            // Successfully parsed the header

            // Check if the file_array capacity needs to be increased
            if (*file_count == *file_capacity) {
                // Increase the capacity by a fixed increment
                *file_capacity += 100;

                // Reallocate the file_array with the new capacity
                struct File *temp = realloc(*file_array, *file_capacity * sizeof(struct File));
                if (temp == NULL) {
                    perror("Failed to reallocate memory for file_array");
                    free(file);  // Free the allocated file struct
                    return -1;
                }
                *file_array = temp;
            }

            // Set the index of the file
            file->index = *file_count;

            // Add the parsed file to the file_array
            (*file_array)[*file_count] = *file;
            (*file_count)++;

            // Increment the refcount of the file
            (*file_array)[file->index].refcount++;
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

    // Dynamic array to hold parsed File structs
    struct File *file_array = NULL;
    size_t file_count = 0;
    size_t file_capacity = 0;

    int result = parse_archive(fd, &file_array, &file_count, &file_capacity);

    if (result == 0) {
        // Parsing successful
        for (size_t i = 0; i < file_count; i++) {
            printf("File Name: %s\n", file_array[i].name);
            printf("File Size: %zu\n", file_array[i].file_size);
            printf("Last Modified Time: %lu\n", file_array[i].last_modified_time);
            printf("Type: %u\n", file_array[i].type);
            printf("File Mode: %u\n", file_array[i].file_mode);
            printf("Owner UID: %u\n", file_array[i].owner_uid);
            printf("Owner GID: %u\n", file_array[i].owner_gid);
            printf("Device Major: %u\n", file_array[i].device_major);
            printf("Device Minor: %u\n", file_array[i].device_minor);
            printf("Header Offset: %zu\n", file_array[i].header_offset);
            // ... Print other file details ...
            printf("\n");
        }
    } else {
        // Parsing failed
        fprintf(stderr, "Failed to parse the archive.\n");
    }

    // Free the dynamically allocated memory for each File struct
    for (size_t i = 0; i < file_count; i++) {
        free_file_buffers(&file_array[i]);
        free(file_array[i].name);
    }

    // Free the file_array itself
    free(file_array);
    close(fd);

    return result;
}