#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

#include <stddef.h>
#include <pmos/ports.h>
#include <stdbool.h>
#include <stdint.h>

struct Filesystem;

enum File_Type {
    FILE,
    FOLDER,
    MOUNTPOINT
}

struct File {
    /// Filesystem owner of the file
    struct Filesystem* parent_fs;

    /// ID of the file within the filesystem
    uint64_t file_id;
};

struct Mountpoint {
    struct File mountpoint_root;
    struct File mount_folder;
};

struct File_Descriptor {
    /// Number of pointers to the descriptor
    size_t refcount;

    enum File_Type type;

    union {
        struct File file;
        struct Mountpoint mountpoint;
    };
};

#endif