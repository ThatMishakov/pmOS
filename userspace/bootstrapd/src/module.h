#pragma once
#include <stdint.h>

struct module_descriptor_list {
    struct module_descriptor_list *next;
    uint64_t object_right;
    size_t size;
    char *cmdline;
    char *path;
    struct Service *service;
};