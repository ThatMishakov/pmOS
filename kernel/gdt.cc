#include "gdt.hh"

constexpr System_Segment_Descriptor::System_Segment_Descriptor() 
    : System_Segment_Descriptor(0, 0, 0, 0)
{}

constexpr System_Segment_Descriptor::System_Segment_Descriptor(uint64_t base, uint32_t limit, uint8_t access, uint8_t flags)
    : limit0(limit & 0xffff),
    base0(base & 0xffff),
    base1((base >> 16) & 0xff),
    access(access),
    limit1((limit >> 16) & 0xff),
    flags(flags),
    base2((base >> 24) & 0xff),
    base3((base >> 32) & 0xffffffff),
    reserved(0)
{}

GDT kernel_gdt;

void init_gdt()
{
    GDT_descriptor gdt_descriptor = {sizeof(GDT) - 1, (uint64_t)&kernel_gdt};
    loadGDT(&gdt_descriptor);
}