#ifndef LIB_INTERNAL_PARAM_H
#define LIB_INTERNAL_PARAM_H 1
#include "kernel/types.h"

#define NOPARAM       0x00
#define ARGS_LIST     0x01
#define ARGS_PLAIN    0x02

// ------------------------- ARGS AS A LIST ------------------------

typedef struct args_list_node {
    u64 size;
    struct args_list_node* next;
} args_list_node;

typedef struct {
    u64 size;
    u64 flags;
    u64 pages;
    args_list_node* p;
} Args_List_Header; 

#define ARGS_LIST_FLAG_ASTEMPPAGE 0x01

#endif