#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <stdint.h>
#include <stddef.h>
#include <pmos/system.h>

struct Filesystem {
    size_t struct_refcount;
    pmos_port_t fs_handler;
};

struct Filesystem * allocate_filesystem_struct();
void free_filesystem_struct(struct Filesystem*);

#endif