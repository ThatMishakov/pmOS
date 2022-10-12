#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H
#include <stdint.h>
#include "types.h"

typedef struct {
    uint16_t limit0;
    uint16_t base0;
    uint8_t base1;
    uint8_t access;
    uint8_t limit1_flags;
    uint8_t base2;
} PACKED GDT_entry;

struct PACKED GDT_descriptor {
    u16 size;
    u64 offset;
};


#endif