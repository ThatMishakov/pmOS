#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <pmos/ports.h>

struct Filesystem {
    uint64_t id;

    // Port used for communication with the filesystem driver
    pmos_port_t command_port;
};

#endif // FILESYSTEM_H