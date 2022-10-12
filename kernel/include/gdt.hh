#ifndef GDT_HH
#define GDT_HH
#include "../common/gdt.h"

struct TSS {
};

struct GDT {
    GDT_entry Null {}; // always null
    GDT_entry _64bit_kernel_code {0, 0, 0, 0x9a, 0xa2, 0};
    GDT_entry _64bit_kernel_data {0, 0, 0, 0x92, 0xa0, 0};
    GDT_entry _64bit_user_code {0, 0, 0, 0xfa, 0xa0, 0};
    GDT_entry _64bit_user_data {0, 0, 0, 0xf2, 0xa0, 0};
} PACKED ALIGNED(0x1000);

extern "C" void loadGDT(GDT_descriptor* GDTdescriptor);

void init_gdt();

#endif