#ifndef KERNEL_COM_H
#define KERNEL_COM_H
#include "types.h"

typedef struct {
    u64 * mem_bitmap;
    u64 mem_bitmap_size;
    u64 flags; /* NX */
} Kernel_Entry_Data;

//#define KERNEL_ADDR_SPACE 01777777750000000000000
#define KERNEL_ADDR_SPACE   0x800000000000//0xFFFF800000000000
#//define USER_ADDR_SPACE   0x800000000000

#endif