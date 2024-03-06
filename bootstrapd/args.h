#ifndef ARGS_H
#define ARGS_H
#include <stdint.h>

typedef struct args_list_node {
    uint64_t size;
    struct args_list_node* next;
} args_list_node;

typedef struct {
    uint64_t size;
    uint64_t flags;
    uint64_t pages;
    args_list_node* p;
} Args_List_Header;

void push_arg(Args_List_Header* header, const char* param);

#endif