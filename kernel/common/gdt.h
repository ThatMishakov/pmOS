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

typedef struct {
    GDT_entry Null; // always null
    GDT_entry _16bit_code;
    GDT_entry _16bit_data;
    GDT_entry _32bit_code;
    GDT_entry _32bit_data;
    GDT_entry _64bit_code;
    GDT_entry _64bit_data;
    GDT_entry IDT_code;
} PACKED ALIGNED(0x1000) GDT;

#endif