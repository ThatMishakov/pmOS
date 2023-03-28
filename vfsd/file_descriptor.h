#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H
#include <stdint.h>
#include <stddef.h>

enum Descriptor_Type {
    Directory,
    File,
};

struct Path_Node;
struct Dir_Data {
    struct Path_Node *child_root;
};

struct File_Descriptor {
    uint64_t id;
    size_t refcount;

    enum Descriptor_Type type;
    union {
        Dir_Data dir_data;

    };
};

#endif