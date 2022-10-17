#ifndef GDT_HH
#define GDT_HH
#include "common/gdt.h"

struct TSS {
    uint32_t reserved0 = 0;
    uint64_t rsp0 = 0;
    uint64_t rsp1 = 0;
    uint64_t rsp2 = 0;
    uint64_t reserved = 0;
    uint64_t ist1 = 0;
    uint64_t ist2 = 0;
    uint64_t ist3 = 0;
    uint64_t ist4 = 0;
    uint64_t ist5 = 0;
    uint64_t ist6 = 0;
    uint64_t ist7 = 0;
    uint64_t reserved1 = 0;
    uint16_t reserved2 = 0;
    uint16_t iopb = 0;
} PACKED;

struct System_Segment_Descriptor {
    uint16_t limit0;
    uint16_t base0;
    uint8_t  base1;
    uint8_t  access;
    uint8_t  limit1: 4;
    uint8_t  flags:  4;
    uint8_t  base2;
    uint32_t base3;
    uint32_t reserved;

    constexpr System_Segment_Descriptor()
        : System_Segment_Descriptor(0, 0, 0, 0) {}
    constexpr System_Segment_Descriptor(uint64_t base, uint32_t limit, uint8_t access, uint8_t flags)
        : limit0(limit & 0xffff), base0(base & 0xffff), base1((base >> 16) & 0xff),
          access(access), limit1((limit >> 16) & 0xff), flags(flags),
          base2((base >> 24) & 0xff), base3((base >> 32) & 0xffffffff),
          reserved(0) {}

    TSS *tss();
};

#define KERNEL_CODE_SELECTOR 0x08

struct GDT {
    GDT_entry Null {}; // always null
    GDT_entry _64bit_kernel_code {0, 0, 0, 0x9a, 0xa2, 0};
    GDT_entry _64bit_kernel_data {0, 0, 0, 0x92, 0xa0, 0};
    GDT_entry _64bit_user_code {0, 0, 0, 0xfa, 0xa0, 0};
    GDT_entry _64bit_user_data {0, 0, 0, 0xf2, 0xa0, 0};
    System_Segment_Descriptor SSD_entries[1] = {};
} PACKED ALIGNED(0x1000);

extern "C" void loadGDT(GDT_descriptor* GDTdescriptor);

void init_gdt();

extern GDT kernel_gdt;

#endif