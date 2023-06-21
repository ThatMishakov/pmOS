#include "file.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    file->name_length = filename_length + prefix_length;

    // Allocate memory for the name
    file->name = malloc(file->name_length + 1);

    // Concatenate the filename and prefix
    strncpy(file->name, header->filename_prefix, prefix_length);
    strncpy(file->name + prefix_length, header->file_name, filename_length);
    file->name[file->name_length] = '\0';

    // Initialize child size and child array
    file->child_size = 0;
    file->child_array = NULL;

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
    if (file->name != NULL) {
        free(file->name);
        file->name = NULL;
    }

    if (file->child_array != NULL) {
        for (size_t i = 0; i < file->child_size; i++) {
            release_file(&file->child_array[i]);
        }
        free(file->child_array);
        file->child_array = NULL;
        file->child_size = 0;
    }
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


