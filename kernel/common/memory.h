#ifndef KENREL_MEMORY_H
#define KENREL_MEMORY_H
#include <stdint.h>

typedef struct {
    uint64_t base_addr;
    uint64_t size;
} memory_descr;

#endif