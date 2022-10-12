#ifndef KERNEL_COM_H
#define KERNEL_COM_H
#include <stdint.h>

typedef struct {
    uint64_t * mem_bitmap;
    uint64_t mem_bitmap_size;
} Kernel_Entry_Data;

#endif