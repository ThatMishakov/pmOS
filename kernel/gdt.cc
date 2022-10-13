#include "gdt.hh"

TSS* System_Segment_Descriptor::tss()
{
    return (TSS*)((uint64_t)base0 | (uint64_t)base1 << 16 | (uint64_t)base2 << 24 | (uint64_t)base3 << 32);
}

GDT kernel_gdt;

void init_gdt()
{
    GDT_descriptor gdt_descriptor = {sizeof(GDT) - 1, (uint64_t)&kernel_gdt};
    loadGDT(&gdt_descriptor);
}