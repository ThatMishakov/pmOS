#pragma once
#include <types.hh>

#define R0_CODE_SEGMENT 0x08

struct GDT {
    u64 null        = 0;
    u64 kernel_code = 0x00cf9a000000ffff;
    u64 kernel_data = 0x00cf92000000ffff;
    u64 ring3_code  = 0x00cffa000000ffff;
    u64 ring3_data  = 0x00cff2000000ffff;
    u64 user_gs     = 0x00cff2000000ffff;
    u64 user_fs     = 0x00cff2000000ffff;
    u64 kernel_gs   = 0x00cff2000000ffff;
    u64 tss         = 0;
};

u32 segment_to_base(u64 segment);
u64 base_to_user_data_segment(u32 base);
u64 base_to_kernel_data_segment(u32 base);
u64 tss_to_base(void *tss);