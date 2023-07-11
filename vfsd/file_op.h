#ifndef FILE_OP_H
#define FILE_OP_H
#include "string.h"
#include <stdint.h>

struct Path_Node;
struct Open_Request {
    // Implicit linked list
    struct Open_Request *next, *prev;

    struct String path;

    const char *active_name;

    struct Path_Node *active_node;

    uint64_t request_id;
    int request_type;
};

#endif // FILE_OP_H