#include "gdt.hh"

#include <types.hh>

u32 segment_to_base(u64 segment)
{
    return ((segment >> 16) & 0xffffff) | ((segment >> 32) & 0xff000000);
}

u64 base_to_user_data_segment(u32 base)
{
    return 0x00cff2000000ffff | (((u64)base & 0xffffff) << 16) | (((u64)base & 0xff000000) << 32);
}