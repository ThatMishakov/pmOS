#include "interrupts.hh"
#include "gdt.hh"
#include "utils.hh"

TSS* System_Segment_Descriptor::tss()
{
    return (TSS*)((uint64_t)base0 | (uint64_t)base1 << 16 | (uint64_t)base2 << 24 | (uint64_t)base3 << 32);
}

GDT kernel_gdt;

void init_gdt()
{
    loadGDT(&kernel_gdt, sizeof(GDT) - 1);
}