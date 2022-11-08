#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H
#include "types.h"

typedef struct {
    u16 limit0;
    u16 base0;
    u8 base1;
    u8 access;
    u8 limit1_flags;
    u8 base2;
} PACKED GDT_entry;

struct PACKED GDT_descriptor {
    u16 size;
    u64 offset;
};


#endif